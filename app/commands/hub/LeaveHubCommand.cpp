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

    const auto channel_ids = ctx.hub_service.getHubChannelIds(hub_id);
    ctx.subscription_manager.unsubscribeConnection(event->conn_id, Topic::HubTopic(hub_id));
    for (const auto& channel_id : channel_ids) {
        ctx.subscription_manager.unsubscribeConnection(event->conn_id,
                                                       Topic::ChannelTopic(hub_id, channel_id));
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

    if (!conns.empty()) {
        std::vector<GlobalConnId> others;
        others.reserve(conns.size());
        for (const auto& conn : conns) {
            if (conn == event->conn_id) {
                continue;
            }
            others.push_back(conn);
        }
        if (!others.empty()) {
            sercom::protocol::event::StateDelta delta;
            auto* hub_delta = delta.add_hubs();
            hub_delta->set_hub_id(hub_id.value);
            hub_delta->add_member_ops()->mutable_remove()->set_user_id(user_id.value);
            out.emplace_back(make_outgoing_message(net::outbound::Target::many(std::move(others)),
                                                   make_state_delta(delta)));
        }
    }

    sercom::protocol::event::StateDelta self_delta;
    auto* self_hub_delta = self_delta.add_hubs();
    self_hub_delta->set_hub_id(hub_id.value);
    self_hub_delta->add_hub_ops()->mutable_remove();
    out.emplace_back(make_outgoing_message(net::outbound::Target::one(event->conn_id),
                                           make_state_delta(self_delta)));
    return out;
}

}  // namespace app
