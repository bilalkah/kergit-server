#include "app/commands/activity/TypingCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "app/proto_builders/EnvelopeBuilders.h"
#include "app/proto_builders/PresenceBuilders.h"
#include "proto/command/activity.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"
#include "proto/event/presence.pb.h"

#include <vector>

namespace app {

std::vector<net::outbound::OutgoingMessage> TypingCommand::execute(CommandContext& ctx,
                                                                   const queue::Event& evt) {
    const auto* event = std::get_if<queue::MessageEvent>(&evt);
    if (!event) {
        return {};
    }

    const auto& env = event->payload.env;
    if (env.type() != sercom::protocol::Envelope::TYPING) {
        return {make_drop_connection(event->conn_id,
                                     sercom::protocol::event::CommandErrorCode_INVALID_FORMAT,
                                     "Invalid TYPING envelope type")};
    }

    const auto& cmd = require_parsed<sercom::protocol::command::Typing>(*event);

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!user_exp.has_value()) {
        return {make_drop_connection(event->conn_id,
                                     sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
                                     "Authenticate first")};
    }
    const UserId user_id = user_exp.value();

    auto channel_id_opt = parse_wire_id<ChannelId>(cmd.channel_id());
    if (!channel_id_opt.has_value()) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_NOT_FOUND,
            "Channel not found"));
    }

    auto channel_opt = ctx.channel_service.getChannel(*channel_id_opt);
    if (!channel_opt.has_value()) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_NOT_FOUND,
            "Channel not found"));
    }
    const HubId hub_id = channel_opt->hub_id;

    if (!ctx.hub_service.isHubMember(hub_id, user_id)) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_FORBIDDEN,
            "Join the hub before typing"));
    }

    if (!connection_has_channel_subscription(ctx.subscription_manager, event->conn_id, hub_id,
                                             *channel_id_opt)) {
        return {};
    }

    sercom::protocol::event::PresenceEvent presence;
    if (cmd.state() == sercom::protocol::command::Typing::STATE_STARTED) {
        presence =
            proto_builders::presence::make_typing_started(user_id.value, channel_id_opt->value);
    } else if (cmd.state() == sercom::protocol::command::Typing::STATE_STOPPED) {
        presence =
            proto_builders::presence::make_typing_stopped(user_id.value, channel_id_opt->value);
    } else {
        return {};
    }

    std::string bytes =
        proto_builders::serialize_envelope(sercom::protocol::Envelope::PRESENCE, presence);

    std::vector<GlobalConnId> conns;
    utils::metrics::counters().fanout_subscriber_snapshot_total.fetch_add(
        1, std::memory_order_relaxed);
    auto subs = ctx.subscription_manager.getSubscribers(Topic::ChannelTopic(hub_id, *channel_id_opt));
    if (subs) {
        for (const auto& conn : *subs) {
            if (conn == event->conn_id) continue;
            conns.push_back(conn);
        }
    }

    if (conns.empty()) {
        return {};
    }

    return single_outgoing(net::outbound::OutgoingMessage{
        .priority = net::outbound::OutboundPriority::Low,
        .target = net::outbound::Target::many(std::move(conns)),
        .action =
            net::outbound::Action{std::in_place_type<net::outbound::SendPayload>,
                                  net::outbound::SendPayload{
                                      .payload = net::outbound::Payload{std::move(bytes), true}}}});
}

}  // namespace app
