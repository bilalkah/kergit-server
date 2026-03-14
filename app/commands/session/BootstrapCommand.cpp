#include "app/commands/session/BootstrapCommand.h"

#include "app/commands/utils.h"
#include "app/managers/subscription/Topic.h"
#include "domains/Channel.h"
#include "domains/Hub.h"
#include "proto/envelope.pb.h"
#include "proto/event/activity.pb.h"
#include "proto/event/session.pb.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace app {

std::vector<net::outbound::OutgoingMessage> BootstrapCommand::execute(CommandContext& ctx,
                                                                      const queue::Event& evt) {
    std::vector<net::outbound::OutgoingMessage> out;
    const auto* event = std::get_if<queue::ConnectionEvent>(&evt);
    if (!event) {
        return out;
    }

    const UserId user_id = event->user_id;
    if (user_id.value.empty()) {
        out.emplace_back(make_drop_connection(
            event->conn_id,
            sercom::protocol::event::CommandErrorCode::CommandErrorCode_UNAUTHORIZED,
            "Authenticate first"));
        return out;
    }

    auto db_user = ctx.user_service.getUser(user_id);
    if (!db_user) {
        out.emplace_back(make_drop_connection(
            event->conn_id,
            sercom::protocol::event::CommandErrorCode::CommandErrorCode_UNAUTHORIZED,
            "User not found"));
        return out;
    }

    auto session_id_exp = ctx.session_manager.sessionIdOfConnection(event->conn_id);
    if (!session_id_exp.has_value()) {
        out.emplace_back(make_drop_connection(
            event->conn_id,
            sercom::protocol::event::CommandErrorCode::CommandErrorCode_UNAUTHORIZED,
            "Session not found"));
        return out;
    }
    const SessionId session_id = session_id_exp.value();

    // Session already created by AuthenticateCommand - just subscribe to hubs
    const auto hubs = ctx.hub_service.getUserHubs(user_id);
    for (const auto& hub : hubs) {
        ctx.subscription_manager.subscribeConnection(event->conn_id, Topic::HubTopic(hub.id));
    }

    // --- build bootstrap payload ---
    sercom::protocol::event::SessionBootstrap bootstrap;

    auto* self = bootstrap.mutable_self();
    self->set_id(user_id.value);

    const std::string display_name =
        !db_user->username.empty() ? db_user->username
                                   : (!db_user->full_name.empty() ? db_user->full_name : "Member");

    self->mutable_metadata()->set_username(display_name);
    self->mutable_metadata()->set_avatar_seed(db_user->avatar_seed);

    for (const auto& hub : hubs) {
        auto* hub_state = bootstrap.add_hubs();

        hub_state->mutable_hub()->set_id(hub.id.value);
        hub_state->mutable_hub()->set_name(hub.name);
        hub_state->mutable_hub()->mutable_metadata()->set_avatar_seed(hub.avatar_seed);

        const auto snapshot = ctx.hub_service.getOrBuildSnapshot(hub.id);
        const auto channels = ctx.channel_service.getHubChannels(hub.id);
        for (const auto& channel : channels) {
            auto* ch = hub_state->add_channels();
            ch->set_id(channel.id.value);
            ch->set_hub_id(hub.id.value);
            ch->set_type(to_proto_channel_type(channel.type));
            ch->mutable_metadata()->set_name(channel.name);
        }

        const auto online = ctx.presence_manager.onlineUsersInHub(hub.id);
        std::unordered_set<UserId> online_set(online.begin(), online.end());

        for (const auto& member : snapshot.members) {
            auto* m = hub_state->add_members();
            m->set_hub_id(hub.id.value);
            m->set_user_id(member.user_id.value);
            m->set_is_online(online_set.contains(member.user_id));
            m->set_role(to_proto_hub_role(member.role));

            // Build user entry for the users lookup table
            if (auto user = ctx.user_service.getUser(member.user_id)) {
                auto* u = hub_state->add_users();
                u->set_id(member.user_id.value);
                const std::string member_display =
                    !user->username.empty()
                        ? user->username
                        : (!user->full_name.empty() ? user->full_name : "Member");
                u->mutable_metadata()->set_username(member_display);
                u->mutable_metadata()->set_avatar_seed(user->avatar_seed);
            }
        }

        for (const auto& channel : channels) {
            if (channel.type != ChannelType::VOICE) continue;
            const auto participants =
                ctx.voice_service.sessions().participants_in_channel(channel.id);
            if (participants.empty()) continue;

            auto* voice_snapshot = bootstrap.add_voice_channels();
            voice_snapshot->mutable_channel()->set_hub_id(hub.id.value);
            voice_snapshot->mutable_channel()->set_channel_id(channel.id.value);
            voice_snapshot->set_started_at_unix(
                ctx.voice_service.channel_started_at_unix(channel.id));
            for (const auto& participant : participants) {
                auto* out_participant = voice_snapshot->add_participants();
                out_participant->set_user_id(participant.user_id.value);
                out_participant->set_muted(participant.muted);
                out_participant->set_deafened(participant.deafened);
            }
        }

        // --- presence fanout ---
        if (!online.empty()) {
            std::unordered_set<GlobalConnId> target_set;
            for (const auto& member_id : online) {
                if (member_id == user_id) continue;
                const auto member_conns = ctx.session_manager.getSessionConnections(member_id);
                for (const auto& conn : member_conns) {
                    target_set.insert(conn);
                }
            }

            std::vector<GlobalConnId> targets;
            targets.reserve(target_set.size());
            for (const auto& conn : target_set) {
                targets.push_back(conn);
            }

            if (!targets.empty()) {
                std::string bytes = make_member_presence(hub.id, user_id, true);

                out.emplace_back(make_outgoing_message(
                    net::outbound::Target::many(std::move(targets)), std::move(bytes)));
            }
        }
    }

    // --- send bootstrap ---
    sercom::protocol::Envelope bootstrap_env;
    bootstrap_env.set_version(1);
    bootstrap_env.set_type(sercom::protocol::Envelope::SESSION_BOOTSTRAP);
    bootstrap.SerializeToString(bootstrap_env.mutable_payload());
    std::string out_bytes = bootstrap_env.SerializeAsString();

    out.emplace_back(
        make_outgoing_message(net::outbound::Target::one(event->conn_id), std::move(out_bytes)));

    sercom::protocol::event::VoiceSelfStatus self_status;
    const auto voice_channel = ctx.voice_service.sessions().user_channel(user_id);
    const auto voice_owner = ctx.voice_service.sessions().user_session(user_id);
    if (voice_channel.has_value()) {
        self_status.set_connected(true);
        self_status.set_is_owner(voice_owner.has_value() && *voice_owner == session_id);
        self_status.set_channel_id(voice_channel->value);
    } else {
        self_status.set_connected(false);
        self_status.set_is_owner(false);
    }

    sercom::protocol::Envelope self_status_env;
    self_status_env.set_version(1);
    self_status_env.set_type(sercom::protocol::Envelope::VOICE_SELF_STATUS);
    self_status.SerializeToString(self_status_env.mutable_payload());
    std::string self_status_bytes = self_status_env.SerializeAsString();
    out.emplace_back(make_outgoing_message(net::outbound::Target::one(event->conn_id),
                                           std::move(self_status_bytes)));
    return out;
}

}  // namespace app
