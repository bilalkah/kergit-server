#include "app/commands/hub/JoinHubByInviteCommand.h"

#include "app/commands/session/StateSyncBuilder.h"
#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "proto/command/hub.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"
#include "proto/event/state.pb.h"

#include <cassert>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace app {

std::vector<net::outbound::OutgoingMessage> JoinHubByInviteCommand::execute(
    CommandContext& ctx, const queue::Event& evt) {
    std::vector<net::outbound::OutgoingMessage> out;
    assert(std::holds_alternative<queue::MessageEvent>(evt));
    const auto* event = &std::get<queue::MessageEvent>(evt);
    const auto& env = event->payload.env;
    assert(env.type() == sercom::protocol::Envelope::HUB_JOIN);

    const auto& cmd = require_parsed<sercom::protocol::command::JoinHub>(*event);

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!user_exp.has_value()) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
            "Authenticate first"));
        return out;
    }
    const UserId user_id = user_exp.value();

    std::string raw_code = cmd.join_code();
    const auto invite_pos = raw_code.find("/invite/");
    const std::string token =
        (invite_pos != std::string::npos) ? raw_code.substr(invite_pos + 8) : raw_code;

    auto hub_id_opt = ctx.invite_service.resolveInvite(token);
    if (!hub_id_opt.has_value()) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVITE_EXPIRED,
            "Invite link is invalid or has expired"));
        return out;
    }
    const HubId hub_id = hub_id_opt.value();

    const auto existing_role = ctx.hub_service.resolveMembershipRole(hub_id, user_id);
    const bool already_member = existing_role.has_value();
    if (!already_member) {
        const auto hub = ctx.hub_service.getHub(hub_id);
        if (!hub.has_value()) {
            out.emplace_back(make_command_error(
                event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_NOT_FOUND,
                "Hub not found"));
            return out;
        }

        try {
            ctx.hub_service.addMember(hub_id, user_id, Role::USER);
        } catch (const std::exception& ex) {
            out.emplace_back(make_command_error(
                event->conn_id, env.type(),
                sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR, ex.what()));
            return out;
        } catch (...) {
            out.emplace_back(make_command_error(
                event->conn_id, env.type(),
                sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR, "Failed to join hub"));
            return out;
        }
    }

    ctx.subscription_manager.subscribeConnection(event->conn_id, Topic::HubTopic(hub_id));

    std::unordered_set<HubId> requested_hub_ids{hub_id};
    const auto sync = build_state_sync_for_requested_hubs(ctx, user_id, requested_hub_ids);
    out.emplace_back(
        make_outgoing_message(net::outbound::Target::one(event->conn_id), make_state_sync(sync)));

    if (already_member) {
        return out;
    }

    utils::metrics::counters().fanout_subscriber_snapshot_total.fetch_add(
        1, std::memory_order_relaxed);
    auto subs = ctx.subscription_manager.getSubscribers(Topic::HubTopic(hub_id));
    if (!subs || subs->empty()) {
        return out;
    }

    std::vector<GlobalConnId> recipients;
    recipients.reserve(subs->size());
    for (const auto& conn : *subs) {
        if (conn == event->conn_id) {
            continue;
        }
        recipients.push_back(conn);
    }
    if (recipients.empty()) {
        return out;
    }

    sercom::protocol::event::StateDelta delta;
    auto* hub_delta = delta.add_hubs();
    hub_delta->set_hub_id(hub_id.value);

    auto* member_upsert = hub_delta->add_member_ops()->mutable_upsert()->mutable_state()->mutable_member();
    *member_upsert = to_proto_hub_member(user_id, std::optional<Role>{Role::USER},
                                         ctx.presence_manager.isUserOnline(user_id));

    auto* user_upsert = hub_delta->add_user_ops()->mutable_upsert()->mutable_state()->mutable_user();
    if (const auto user = ctx.user_service.getUser(user_id)) {
        *user_upsert = to_proto_user(*user);
    } else {
        user_upsert->set_id(user_id.value);
    }

    out.emplace_back(make_outgoing_message(net::outbound::Target::many(std::move(recipients)),
                                           make_state_delta(delta)));
    return out;
}

}  // namespace app
