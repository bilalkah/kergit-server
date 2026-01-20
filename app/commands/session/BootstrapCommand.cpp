#include "app/commands/session/BootstrapCommand.h"

#include "app/converters/ProtoConverters.h"
#include "app/managers/subscription/Topic.h"
#include "domains/Channel.h"
#include "domains/Hub.h"
#include "proto/envelope.pb.h"
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
        std::cout << "BootstrapCommand: invalid event type" << std::endl;
        return out;
    }

    const UserId user_id = event->user_id;
    if (user_id.value.empty()) {
        return {net::outbound::OutgoingMessage{
            .target = net::outbound::Target::one(event->conn_id),
            .action = net::outbound::DropConnection{
                .code = static_cast<int>(
                    sercom::protocol::event::CommandErrorCode::CommandErrorCode_UNAUTHORIZED),
                .reason = "missing_user_id"}}};
    }

    auto db_user = ctx.user_service.getUser(user_id);
    if (!db_user) {
        return {net::outbound::OutgoingMessage{
            .target = net::outbound::Target::one(event->conn_id),
            .action = net::outbound::DropConnection{
                .code = static_cast<int>(
                    sercom::protocol::event::CommandErrorCode::CommandErrorCode_UNAUTHORIZED),
                .reason = "User not found"}}};
    }

    if (ctx.session_manager.hasSession(user_id)) {
        return {net::outbound::OutgoingMessage{
            .target = net::outbound::Target::one(event->conn_id),
            .action = net::outbound::DropConnection{
                .code = static_cast<int>(
                    sercom::protocol::event::CommandErrorCode::CommandErrorCode_INVALID_SESSION),
                .reason = "duplicate_session"}}};
    }

    // --- session + subscriptions ---
    ctx.session_manager.createSession(event->conn_id, user_id);

    const auto hubs = ctx.hub_service.getUserHubs(user_id);
    for (const auto& hub : hubs) {
        ctx.subscription_manager.subscribe(user_id, Topic::HubTopic(hub.id));
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
            m->set_display_name(member.display_name.empty() ? "Member" : member.display_name);
            m->set_role(converters::to_proto_hub_role(member.role));
            m->set_avatar_seed(member.avatar_seed);
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
                sercom::protocol::event::PresenceEvent pe;
                auto* pc = pe.mutable_presence_changed();
                pc->set_hub_id(public_hub_id.value);
                pc->set_user_id(public_user_id.value);
                pc->set_is_online(true);

                std::string pe_payload;
                pe.SerializeToString(&pe_payload);

                sercom::protocol::Envelope penv;
                penv.set_version(1);
                penv.set_type(sercom::protocol::Envelope::PRESENCE);
                penv.set_payload(std::move(pe_payload));

                std::string bytes;
                penv.SerializeToString(&bytes);

                out.push_back(net::outbound::OutgoingMessage{
                    .target = net::outbound::Target::many(std::move(targets)),
                    .action = net::outbound::SendPayload{
                        .payload =
                            net::outbound::Payload{.data = std::move(bytes), .is_binary = true}}});
            }
        }
    }

    // --- send bootstrap ---
    std::string bootstrap_payload;
    bootstrap.SerializeToString(&bootstrap_payload);

    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(sercom::protocol::Envelope::SESSION_BOOTSTRAP);
    env.set_payload(std::move(bootstrap_payload));

    std::string out_bytes;
    env.SerializeToString(&out_bytes);

    out.push_back(net::outbound::OutgoingMessage{
        .target = net::outbound::Target::one(event->conn_id),
        .action = net::outbound::SendPayload{
            .payload = net::outbound::Payload{.data = std::move(out_bytes), .is_binary = true}}});

    return out;
}

}  // namespace app
