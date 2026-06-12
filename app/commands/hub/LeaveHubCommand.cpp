#include "app/commands/hub/LeaveHubCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "domains/Hub.h"
#include "proto/command/hub.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"
#include "proto/event/state.pb.h"

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
        out.emplace_back(make_command_error(event->conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
                                            "Authenticate first"));
        return out;
    }

    const UserId user_id = user_exp.value();
    const HubId hub_id{cmd.hub_id()};
    auto role = ctx.hub_service.getMembershipRole(hub_id, user_id);
    if (!role) {
        out.emplace_back(make_command_error(event->conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                            "Join the hub before leaving it"));
        return out;
    }
    if (*role == Role::OWNER) {
        out.emplace_back(make_command_error(event->conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                            "Owners must transfer ownership before leaving"));
        return out;
    }

    try {
        ctx.hub_service.removeMember(hub_id, user_id);
    } catch (const std::exception&) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
            "Unable to leave hub at this time"));
        return out;
    } catch (...) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
            "Failed to leave hub"));
        return out;
    }

    auto self_conns = ctx.session_manager.getSessionConnections(user_id);
    if (self_conns.empty()) {
        self_conns.push_back(event->conn_id);
    }

    const auto channel_ids = ctx.hub_service.getHubChannelIds(hub_id);
    for (const auto& conn : self_conns) {
        ctx.subscription_manager.unsubscribeConnection(conn, Topic::HubTopic(hub_id));
        for (const auto& channel_id : channel_ids) {
            ctx.subscription_manager.unsubscribeConnection(conn,
                                                           Topic::ChannelTopic(hub_id, channel_id));
        }
    }

    std::vector<GlobalConnId> recipients;
    utils::metrics::counters().fanout_subscriber_snapshot_total.fetch_add(
        1, std::memory_order_relaxed);
    auto subs = ctx.subscription_manager.getSubscribers(Topic::HubTopic(hub_id));
    if (subs) {
        recipients.reserve(subs->size());
        for (const auto& conn : *subs) {
            recipients.push_back(conn);
        }
    }

    if (!recipients.empty()) {
        sercom::protocol::event::StateDelta delta;
        auto* hub_delta = delta.add_hubs();
        hub_delta->set_hub_id(hub_id.value);
        hub_delta->add_member_ops()->mutable_remove()->set_user_id(user_id.value);
        out.emplace_back(make_outgoing_message(net::outbound::Target::many(std::move(recipients)),
                                               make_state_delta(delta)));
    }

    sercom::protocol::event::StateDelta self_delta;
    auto* self_hub_delta = self_delta.add_hubs();
    self_hub_delta->set_hub_id(hub_id.value);
    self_hub_delta->add_hub_ops()->mutable_remove();
    out.emplace_back(make_outgoing_message(net::outbound::Target::many(std::move(self_conns)),
                                           make_state_delta(self_delta)));
    return out;
}

}  // namespace app
