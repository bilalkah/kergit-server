#include "app/commands/channel/RemoveChannelCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "domains/Channel.h"
#include "domains/Hub.h"
#include "proto/command/channel.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"

#include <cassert>
#include <optional>
#include <vector>

namespace app {

std::vector<net::outbound::OutgoingMessage> RemoveChannelCommand::execute(CommandContext& ctx,
                                                                          const queue::Event& evt) {
    std::vector<net::outbound::OutgoingMessage> out;
    assert(std::holds_alternative<queue::MessageEvent>(evt));
    const auto* event = &std::get<queue::MessageEvent>(evt);
    const auto& env = event->payload.env;
    assert(env.type() == sercom::protocol::Envelope::CHANNEL_REMOVE);

    const auto& cmd = require_parsed<sercom::protocol::command::RemoveChannel>(*event);

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!user_exp.has_value()) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
            "Authenticate first"));
        return out;
    }
    const UserId user_id = user_exp.value();
    const ChannelId channel_id{cmd.channel_id()};
    
    auto channel_opt = ctx.channel_service.getChannel(channel_id);
    if (!channel_opt.has_value()) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_NOT_FOUND,
            "Channel not found"));
        return out;
    }
    const Channel channel = channel_opt.value();
    const HubId hub_id = channel.hub_id;

    if (!ctx.hub_service.isHubMember(hub_id, user_id)) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_FORBIDDEN,
            "Join the hub before removing channels"));
        return out;
    }

    auto role = ctx.hub_service.getMembershipRole(hub_id, user_id);
    if (!role.has_value() || *role != Role::OWNER) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_FORBIDDEN,
            "Only owners can delete channels"));
        return out;
    }

    if (!ctx.channel_service.deleteChannel(channel.id, hub_id)) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
            "Unable to delete channel at this time"));
        return out;
    }

    ctx.subscription_manager.removeAllForTopic(Topic::ChannelTopic(hub_id, channel.id));

    std::string bytes = make_channel_remove(hub_id, channel.id);

    utils::metrics::counters().fanout_subscriber_snapshot_total.fetch_add(
        1, std::memory_order_relaxed);
    auto subs = ctx.subscription_manager.getSubscribers(Topic::HubTopic(hub_id));
    if (!subs || subs->empty()) {
        return {};
    }

    std::vector<GlobalConnId> conns;
    conns.reserve(subs->size());
    for (const auto& conn : *subs) {
        conns.push_back(conn);
    }
    if (conns.empty()) {
        return {};
    }

    out.emplace_back(make_outgoing_message(net::outbound::Target::many(std::move(conns)), std::move(bytes)));
    return out;
}

}  // namespace app
