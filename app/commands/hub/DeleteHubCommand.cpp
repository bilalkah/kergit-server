#include "app/commands/hub/DeleteHubCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "domains/Channel.h"
#include "domains/Hub.h"
#include "proto/command/hub.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"
#include "proto/event/hub.pb.h"

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
    if (!ctx.hub_service.isHubMember(hub_id, user_id)) {
        out.emplace_back(make_command_error(event->conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                            "Join the hub before removing it"));
        return out;
    }

    auto role = ctx.hub_service.getMembershipRole(hub_id, user_id);
    if (!role || *role != Role::OWNER) {
        out.emplace_back(make_command_error(event->conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                            "Only owners can remove hubs"));
        return out;
    }

    const auto channels = ctx.channel_service.getHubChannels(hub_id);

    try {
        if (!ctx.hub_service.deleteHub(hub_id, user_id)) {
            out.emplace_back(
                make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
                                   "Unable to remove hub at this time"));
            return out;
        }
    } catch (const std::exception& ex) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
            ex.what()));
        return out;
    }

    utils::metrics::counters().fanout_subscriber_snapshot_total.fetch_add(
        1, std::memory_order_relaxed);
    auto subs = ctx.subscription_manager.getSubscribers(Topic::HubTopic(hub_id));
    std::vector<GlobalConnId> conns;
    conns.reserve(subs->size());
    for (const auto& conn : *subs) {
        conns.push_back(conn);
    }

    std::string bytes = make_hub_remove(hub_id);

    ctx.subscription_manager.removeAllForTopic(Topic::HubTopic(hub_id));
    for (const auto& ch : channels) {
        ctx.subscription_manager.removeAllForTopic(Topic::ChannelTopic(hub_id, ch.id));
    }

    out.emplace_back(make_outgoing_message(net::outbound::Target::many(conns), std::move(bytes)));
    return out;
}

}  // namespace app
