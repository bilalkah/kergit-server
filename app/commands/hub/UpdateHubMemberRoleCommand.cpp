#include "app/commands/hub/UpdateHubMemberRoleCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "app/services/hub/RolePolicy.h"
#include "proto/command/hub.pb.h"
#include "proto/domain/hub.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"
#include "proto/event/state.pb.h"

#include <cassert>
#include <exception>
#include <optional>
#include <vector>

namespace app {

namespace {
std::optional<Role> parse_target_role(const sercom::protocol::domain::HubRole role) {
    using ProtoHubRole = sercom::protocol::domain::HubRole;
    switch (role) {
        case ProtoHubRole::HubRole_ADMIN:
            return Role::ADMIN;
        case ProtoHubRole::HubRole_MEMBER:
            return Role::USER;
        default:
            return std::nullopt;
    }
}
}  // namespace

std::vector<net::outbound::OutgoingMessage> UpdateHubMemberRoleCommand::execute(
    CommandContext& ctx, const queue::Event& evt) {
    std::vector<net::outbound::OutgoingMessage> out;
    assert(std::holds_alternative<queue::MessageEvent>(evt));
    const auto* event = &std::get<queue::MessageEvent>(evt);
    const auto& env = event->payload.env;
    assert(env.type() == sercom::protocol::Envelope::HUB_MEMBER_ROLE_UPDATE);

    const auto& cmd = require_parsed<sercom::protocol::command::UpdateHubMemberRole>(*event);

    auto actor_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!actor_exp.has_value()) {
        out.emplace_back(make_command_error(event->conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
                                            "Authenticate first"));
        return out;
    }
    const UserId actor_id = actor_exp.value();

    const HubId hub_id{cmd.hub_id()};
    const UserId target_id{cmd.user_id()};
    if (hub_id.value.empty() || target_id.value.empty()) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "hub_id and user_id are required"));
        return out;
    }

    const auto desired_role_opt = parse_target_role(cmd.role());
    if (!desired_role_opt.has_value()) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "Role must be MEMBER or ADMIN"));
        return out;
    }
    const Role desired_role = *desired_role_opt;

    auto actor_role = ctx.hub_service.getMembershipRole(hub_id, actor_id);
    if (!actor_role.has_value()) {
        out.emplace_back(make_command_error(event->conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                            "Join the hub before updating member roles"));
        return out;
    }

    auto target_role = ctx.hub_service.getMembershipRole(hub_id, target_id);
    if (!target_role.has_value()) {
        out.emplace_back(make_command_error(event->conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_NOT_FOUND,
                                            "Target user is not a member"));
        return out;
    }

    if (!services::hub::canChangeMemberRole(*actor_role, *target_role, desired_role, actor_id,
                                            target_id)) {
        out.emplace_back(make_command_error(event->conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                            "You are not allowed to update this member role"));
        return out;
    }

    try {
        ctx.hub_service.addMember(hub_id, target_id, desired_role);
    } catch (const std::exception&) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
            "Unable to update member role at this time"));
        return out;
    } catch (...) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
            "Failed to update member role"));
        return out;
    }

    utils::metrics::counters().fanout_subscriber_snapshot_total.fetch_add(
        1, std::memory_order_relaxed);
    const auto subs = ctx.subscription_manager.getSubscribers(Topic::HubTopic(hub_id));

    sercom::protocol::event::StateDelta delta;
    auto* hub_delta = delta.add_hubs();
    hub_delta->set_hub_id(hub_id.value);
    auto* upsert = hub_delta->add_member_ops()->mutable_upsert()->mutable_state()->mutable_member();
    *upsert = to_proto_hub_member(target_id, std::optional<Role>{desired_role},
                                  ctx.presence_manager.isUserOnline(target_id));

    if (subs && !subs->empty()) {
        std::vector<GlobalConnId> recipients{subs->begin(), subs->end()};
        out.emplace_back(make_outgoing_message(net::outbound::Target::many(std::move(recipients)),
                                               make_state_delta(delta)));
        return out;
    }

    out.emplace_back(
        make_outgoing_message(net::outbound::Target::one(event->conn_id), make_state_delta(delta)));
    return out;
}

}  // namespace app
