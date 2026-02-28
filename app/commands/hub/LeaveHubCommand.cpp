#include "app/commands/hub/LeaveHubCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "app/proto_builders/EnvelopeBuilders.h"
#include "domains/Hub.h"
#include "proto/command/hub.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"
#include "proto/event/hub.pb.h"

#include <vector>

namespace app {

std::vector<net::outbound::OutgoingMessage> LeaveHubCommand::execute(CommandContext& ctx,
                                                                     const queue::Event& evt) {
    const auto* event = std::get_if<queue::MessageEvent>(&evt);
    if (!event) {
        return {};
    }

    const auto& env = event->payload.env;
    if (env.type() != sercom::protocol::Envelope::HUB_LEAVE) {
        return {};
    }

    const auto& cmd = require_parsed<sercom::protocol::command::LeaveHub>(*event);

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!user_exp.has_value()) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
            "Authenticate first"));
    }
    const UserId user_id = user_exp.value();

    auto hub_id_opt = parse_wire_id<HubId>(cmd.hub_id());
    if (!hub_id_opt.has_value()) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_NOT_FOUND,
            "Hub not found"));
    }
    const HubId hub_id = hub_id_opt.value();

    auto role = ctx.hub_service.getMembershipRole(hub_id, user_id);
    if (!role) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_FORBIDDEN,
            "Join the hub before leaving it"));
    }
    if (*role == Role::OWNER) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_FORBIDDEN,
            "Owners must transfer ownership before leaving"));
    }

    try {
        ctx.hub_service.removeMember(hub_id, user_id);
    } catch (const std::exception& ex) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
            ex.what()));
    } catch (...) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
            "Failed to leave hub"));
    }

    const auto channels = ctx.channel_service.getHubChannels(hub_id);
    ctx.subscription_manager.unsubscribeConnection(event->conn_id, Topic::HubTopic(hub_id));
    for (const auto& ch : channels) {
        ctx.subscription_manager.unsubscribeConnection(event->conn_id,
                                                       Topic::ChannelTopic(hub_id, ch.id));
    }

    std::vector<GlobalConnId> conns;
    utils::metrics::counters().fanout_subscriber_snapshot_total.fetch_add(
        1, std::memory_order_relaxed);
    auto subs = ctx.subscription_manager.getSubscribers(Topic::HubTopic(hub_id));
    if (subs) {
        conns.reserve(subs->size() + 1);
        for (const auto& conn : *subs) {
            conns.push_back(conn);
        }
    }

    bool exists = false;
    for (const auto& existing : conns) {
        if (existing == event->conn_id) {
            exists = true;
            break;
        }
    }
    if (!exists) {
        conns.push_back(event->conn_id);
    }

    if (conns.empty()) {
        return {};
    }

    sercom::protocol::event::HubMemberLeft left;
    left.set_hub_id(hub_id.value);
    left.set_user_id(user_id.value);

    std::string bytes =
        proto_builders::serialize_envelope(sercom::protocol::Envelope::HUB_MEMBER_LEFT, left);

    return single_outgoing(net::outbound::OutgoingMessage{
        .target = net::outbound::Target::many(std::move(conns)),
        .action =
            net::outbound::Action{std::in_place_type<net::outbound::SendPayload>,
                                  net::outbound::SendPayload{
                                      .payload = net::outbound::Payload{std::move(bytes), true}}}});
}

}  // namespace app
