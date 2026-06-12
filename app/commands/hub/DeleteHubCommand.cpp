#include "app/commands/hub/DeleteHubCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "domains/Channel.h"
#include "domains/Hub.h"
#include "proto/command/hub.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"
#include "proto/event/state.pb.h"

#include <cassert>
#include <vector>

namespace app {

std::vector<net::outbound::OutgoingMessage> DeleteHubCommand::execute(CommandContext& ctx,
                                                                      const queue::Event& evt) {
    std::vector<net::outbound::OutgoingMessage> out;
    assert(std::holds_alternative<queue::MessageEvent>(evt));
    const auto* event = &std::get<queue::MessageEvent>(evt);
    const auto& env = event->payload.env;
    assert(env.type() == sercom::protocol::Envelope::HUB_REMOVE);

    const auto& cmd = require_parsed<sercom::protocol::command::RemoveHub>(*event);

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!user_exp.has_value()) {
        out.emplace_back(make_drop_connection(event->conn_id,
                                              sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                              "Authenticate first"));
        return out;
    }

    const UserId user_id = user_exp.value();
    const HubId hub_id{cmd.hub_id()};
    auto role = ctx.hub_service.getMembershipRole(hub_id, user_id);
    if (!role) {
        out.emplace_back(make_command_error(event->conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                            "Join the hub before removing it"));
        return out;
    }

    if (*role != Role::OWNER) {
        out.emplace_back(make_command_error(event->conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                            "Only owners can remove hubs"));
        return out;
    }

    const auto channel_ids = ctx.hub_service.getHubChannelIds(hub_id);

    try {
        if (!ctx.hub_service.deleteHub(hub_id, user_id)) {
            out.emplace_back(
                make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
                                   "Unable to remove hub at this time"));
            return out;
        }
        ctx.invite_service.revokeInvitesForHub(hub_id);
    } catch (const std::exception&) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
            "Unable to delete hub at this time"));
        return out;
    }

    utils::metrics::counters().fanout_subscriber_snapshot_total.fetch_add(
        1, std::memory_order_relaxed);
    auto subs = ctx.subscription_manager.getSubscribers(Topic::HubTopic(hub_id));
    std::vector<GlobalConnId> conns;
    if (subs) {
        conns.reserve(subs->size());
        for (const auto& conn : *subs) {
            conns.push_back(conn);
        }
    }

    sercom::protocol::event::StateDelta delta;
    auto* hub_delta = delta.add_hubs();
    hub_delta->set_hub_id(hub_id.value);
    hub_delta->add_hub_ops()->mutable_remove();

    ctx.subscription_manager.removeAllForTopic(Topic::HubTopic(hub_id));
    for (const auto& channel_id : channel_ids) {
        ctx.subscription_manager.removeAllForTopic(Topic::ChannelTopic(hub_id, channel_id));
    }

    if (!conns.empty()) {
        out.emplace_back(make_outgoing_message(net::outbound::Target::many(std::move(conns)),
                                               make_state_delta(delta)));
    }
    return out;
}

}  // namespace app
