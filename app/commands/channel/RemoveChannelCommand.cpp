#include "app/commands/channel/RemoveChannelCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "domains/Channel.h"
#include "domains/Hub.h"
#include "proto/command/channel.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"

#include <optional>
#include <vector>

namespace app {

std::vector<net::outbound::OutgoingMessage> RemoveChannelCommand::execute(CommandContext& ctx,
                                                                          const queue::Event& evt) {
    const auto* event = std::get_if<queue::MessageEvent>(&evt);
    if (!event) {
        return {};
    }

    const auto& env = event->payload.env;
    if (env.type() != sercom::protocol::Envelope::CHANNEL_REMOVE) {
        return {};
    }

    const auto& cmd = require_parsed<sercom::protocol::command::RemoveChannel>(*event);

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!user_exp.has_value()) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
                                   "Authenticate first"));
    }
    const UserId user_id = user_exp.value();

    auto hub_id_opt = ctx.ids.to_internal(PublicHubId{cmd.hub_id()});
    if (!hub_id_opt.has_value()) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_NOT_FOUND,
                                   "Hub not found"));
    }
    const HubId hub_id = hub_id_opt.value();

    auto channel_id_opt = ctx.ids.to_internal(PublicChannelId{cmd.channel_id()});
    if (!channel_id_opt.has_value()) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_NOT_FOUND,
                                   "Channel not found"));
    }

    auto channel_opt = ctx.channel_service.getChannel(*channel_id_opt);
    if (!channel_opt.has_value() || channel_opt->hub_id != hub_id) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_NOT_FOUND,
                                   "Channel not found"));
    }
    const Channel channel = channel_opt.value();

    if (!ctx.hub_service.isHubMember(hub_id, user_id)) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                   "Join the hub before removing channels"));
    }

    auto role = ctx.hub_service.getMembershipRole(hub_id, user_id);
    if (!role.has_value() || *role != Role::OWNER) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                   "Only owners can delete channels"));
    }

    if (!ctx.channel_service.deleteChannel(channel.id, hub_id)) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
                                   "Unable to delete channel at this time"));
    }

    ctx.subscription_manager.removeAllForTopic(Topic::ChannelTopic(hub_id, channel.id));

    std::string bytes = ctx.hub_notifier.channelDeleted(hub_id, channel.id);

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

    return single_outgoing(net::outbound::OutgoingMessage{
        .target = net::outbound::Target::many(std::move(conns)),
        .action =
            net::outbound::Action{std::in_place_type<net::outbound::SendPayload>,
                                  net::outbound::SendPayload{.payload = net::outbound::Payload{std::move(bytes), true}}}});
}

}  // namespace app
