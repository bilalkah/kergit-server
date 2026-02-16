#include "app/commands/session/BootstrapCommand.h"

#include "app/commands/utils.h"
#include "app/converters/ProtoConverters.h"
#include "app/managers/subscription/Topic.h"
#include "app/proto_builders/EnvelopeBuilders.h"
#include "app/proto_builders/PresenceBuilders.h"
#include "domains/Channel.h"
#include "domains/Hub.h"
#include "proto/envelope.pb.h"
#include "proto/event/activity.pb.h"
#include "proto/event/error.pb.h"
#include "proto/event/presence.pb.h"
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
        return single_outgoing(net::outbound::OutgoingMessage{
            .target = net::outbound::Target::one(event->conn_id),
            .action = net::outbound::Action{
                std::in_place_type<net::outbound::DropConnection>,
                static_cast<int>(
                    sercom::protocol::event::CommandErrorCode::CommandErrorCode_UNAUTHORIZED),
                "missing_user_id"}});
    }

    auto db_user = ctx.user_service.getUser(user_id);
    if (!db_user) {
        return single_outgoing(net::outbound::OutgoingMessage{
            .target = net::outbound::Target::one(event->conn_id),
            .action = net::outbound::Action{
                std::in_place_type<net::outbound::DropConnection>,
                static_cast<int>(
                    sercom::protocol::event::CommandErrorCode::CommandErrorCode_UNAUTHORIZED),
                "User not found"}});
    }

    // Session already created by AuthenticateCommand - just subscribe to hubs
    const auto hubs = ctx.hub_service.getUserHubs(user_id);
    for (const auto& hub : hubs) {
        ctx.subscription_manager.subscribeConnection(event->conn_id, Topic::HubTopic(hub.id));
    }

    // --- build bootstrap payload ---
    sercom::protocol::event::SessionBootstrap bootstrap;

    const auto public_user_id = ctx.ids.to_public(user_id);
    auto* self = bootstrap.mutable_self();
    self->set_id(public_user_id.value);

    const std::string display_name =
        !db_user->username.empty() ? db_user->username
                                   : (!db_user->full_name.empty() ? db_user->full_name : "Member");

    self->set_username(display_name);
    self->set_avatar_seed(db_user->avatar_seed);

    for (const auto& hub : hubs) {
        auto* hub_state = bootstrap.add_hubs();
        const auto public_hub_id = ctx.ids.to_public(hub.id);

        hub_state->mutable_hub()->set_id(public_hub_id.value);
        hub_state->mutable_hub()->set_name(hub.name);
        hub_state->mutable_hub()->set_avatar_seed(hub.avatar_seed);

        const auto snapshot = ctx.hub_service.getOrBuildSnapshot(hub.id);
        for (const auto& channel : snapshot.channels) {
            auto* ch = hub_state->add_channels();
            ch->set_id(ctx.ids.to_public(channel.id).value);
            ch->set_name(channel.name);
            ch->set_type(converters::to_proto_channel_type(channel.type));
        }

        const auto online = ctx.presence_manager.onlineUsersInHub(hub.id);
        std::unordered_set<UserId> online_set(online.begin(), online.end());

        for (const auto& member : snapshot.members) {
            auto* m = hub_state->add_members();
            m->set_hub_id(public_hub_id.value);
            m->set_user_id(ctx.ids.to_public(member.user_id).value);
            m->set_is_online(online_set.contains(member.user_id));
            m->set_role(converters::to_proto_hub_role(member.role));

            // Fetch fresh user info from UserService (uses cache if available)
            if (auto user = ctx.user_service.getUser(member.user_id)) {
                const std::string member_display =
                    !user->username.empty()
                        ? user->username
                        : (!user->full_name.empty() ? user->full_name : "Member");
                m->set_display_name(member_display);
                m->set_avatar_seed(user->avatar_seed);
            } else {
                m->set_display_name("Member");
                m->set_avatar_seed("");
            }
        }

        for (const auto& channel : snapshot.channels) {
            if (channel.type != ChannelType::VOICE) continue;
            const auto participants =
                ctx.session_manager.voiceParticipantsInChannel(hub.id, channel.id);
            if (participants.empty()) continue;

            auto* voice_snapshot = bootstrap.add_voice_channels();
            voice_snapshot->set_hub_id(public_hub_id.value);
            voice_snapshot->set_channel_id(ctx.ids.to_public(channel.id).value);
            for (const auto& uid : participants) {
                voice_snapshot->add_participant_user_ids(ctx.ids.to_public(uid).value);
            }
        }

        // --- presence fanout ---
        if (!online.empty()) {
            std::vector<GlobalConnId> targets;
            for (const auto& member_id : online) {
                if (member_id == user_id) continue;
                auto conn = ctx.session_manager.getMainConnection(member_id);
                if (conn) targets.push_back(*conn);
            }

            if (!targets.empty()) {
                auto pe = proto_builders::presence::make_presence_changed(
                    public_hub_id.value, public_user_id.value, true);
                std::string bytes =
                    proto_builders::serialize_envelope(sercom::protocol::Envelope::PRESENCE, pe);

                out.emplace_back();
                auto& msg = out.back();
                msg.priority = net::outbound::OutboundPriority::Low;
                msg.target = net::outbound::Target::many(std::move(targets));
                msg.action.emplace<net::outbound::SendPayload>(net::outbound::SendPayload{
                    .payload = net::outbound::Payload{std::move(bytes), true}});
            }
        }
    }

    // --- send bootstrap ---
    std::string out_bytes = proto_builders::serialize_envelope(
        sercom::protocol::Envelope::SESSION_BOOTSTRAP, bootstrap);

    out.emplace_back();
    auto& msg = out.back();
    msg.target = net::outbound::Target::one(event->conn_id);
    msg.action.emplace<net::outbound::SendPayload>(
        net::outbound::SendPayload{.payload = net::outbound::Payload{std::move(out_bytes), true}});

    return out;
}

}  // namespace app
