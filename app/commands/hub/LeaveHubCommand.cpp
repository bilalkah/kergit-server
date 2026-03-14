#include "app/commands/hub/LeaveHubCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "domains/Hub.h"
#include "proto/command/hub.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"
#include "proto/event/hub.pb.h"

#include <cassert>
#include <vector>

namespace app {

std::vector<net::outbound::OutgoingMessage> LeaveHubCommand::execute(CommandContext& ctx,
                                                                     const queue::Event& evt) {
    std::vector<net::outbound::OutgoingMessage> out;
    assert(std::holds_alternative<queue::MessageEvent>(evt));
    const auto* event = &std::get<queue::MessageEvent>(evt);
    const auto& env = event->payload.env;
    assert(env.type() == sercom::protocol::Envelope::HUB_LEAVE);

    const auto& cmd = require_parsed<sercom::protocol::command::LeaveHub>(*event);

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!user_exp.has_value()) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
            "Authenticate first"));
        return out;
    }
    
    const UserId user_id = user_exp.value();
    const HubId hub_id{cmd.hub_id()};
    auto role = ctx.hub_service.getMembershipRole(hub_id, user_id);
    if (!role) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_FORBIDDEN,
            "Join the hub before leaving it"));
        return out;
    }
    if (*role == Role::OWNER) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_FORBIDDEN,
            "Owners must transfer ownership before leaving"));
        return out;
    }

    try {
        ctx.hub_service.removeMember(hub_id, user_id);
    } catch (const std::exception& ex) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
            ex.what()));
        return out;
    } catch (...) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
            "Failed to leave hub"));
        return out;
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

    sercom::protocol::Envelope out_env;
    out_env.set_version(1);
    out_env.set_type(sercom::protocol::Envelope::HUB_MEMBER_LEFT);
    left.SerializeToString(out_env.mutable_payload());
    std::string bytes = out_env.SerializeAsString();

    out.emplace_back(net::outbound::OutgoingMessage{
        .target = net::outbound::Target::many(std::move(conns)),
        .action =
            net::outbound::Action{std::in_place_type<net::outbound::SendPayload>,
                                  net::outbound::SendPayload{
                                      .payload = net::outbound::Payload{std::move(bytes), true}}}});
    return out;
}

}  // namespace app
