#include "app/commands/activity/TypingCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "proto/command/activity.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"
#include "proto/event/realtime.pb.h"

#include <cassert>
#include <string>
#include <utility>
#include <vector>

namespace app {
namespace {

std::string make_typing_signal(const HubId& hub_id, const ChannelId& channel_id,
                               const UserId& user_id, bool is_typing) {
    sercom::protocol::event::RtSignal signal;
    auto* payload = signal.mutable_typing();
    *payload->mutable_channel() = to_proto_channel_ref(hub_id, channel_id);
    payload->set_user_id(user_id.value);
    payload->set_is_typing(is_typing);
    return make_rt_signal(signal);
}

}  // namespace

std::vector<net::outbound::OutgoingMessage> TypingCommand::execute(CommandContext& ctx,
                                                                   const queue::Event& evt) {
    std::vector<net::outbound::OutgoingMessage> out;
    assert(std::holds_alternative<queue::MessageEvent>(evt));
    const auto* event = &std::get<queue::MessageEvent>(evt);
    const auto& env = event->payload.env;
    assert(env.type() == sercom::protocol::Envelope::TYPING);

    const auto& cmd = require_parsed<sercom::protocol::command::Typing>(*event);
    const auto scope_opt = to_channel_scope(cmd.channel());
    if (!scope_opt.has_value()) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "channel.hub_id and channel.channel_id are required"));
        return out;
    }
    const HubId hub_id = scope_opt->hub_id;
    const ChannelId ch_id = scope_opt->channel_id;

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!user_exp.has_value()) {
        out.emplace_back(make_drop_connection(event->conn_id,
                                              sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
                                              "Authenticate first"));
        return out;
    }
    const UserId user_id = user_exp.value();

    auto channel_opt = ctx.hub_service.getChannel(ch_id);
    if (!channel_opt || channel_opt->hub_id != hub_id) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_NOT_FOUND,
            "Channel not found"));
        return out;
    }

    if (!ctx.hub_service.isHubMember(hub_id, user_id)) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_FORBIDDEN,
            "Join the hub before typing"));
        return out;
    }

    if (!ctx.subscription_manager.isSubscribed(event->conn_id, Topic::ChannelTopic(hub_id, ch_id))) {
        return {};
    }

    std::string bytes = make_typing_signal(hub_id, ch_id, user_id, cmd.is_typing());

    std::vector<GlobalConnId> conns;
    utils::metrics::counters().fanout_subscriber_snapshot_total.fetch_add(
        1, std::memory_order_relaxed);
    auto subs = ctx.subscription_manager.getSubscribers(Topic::ChannelTopic(hub_id, ch_id));
    if (subs) {
        for (const auto& conn : *subs) {
            if (conn == event->conn_id) continue;
            conns.push_back(conn);
        }
    }

    if (conns.empty()) {
        return {};
    }

    auto msg = make_outgoing_message(net::outbound::Target::many(std::move(conns)), std::move(bytes));
    msg.priority = net::outbound::OutboundPriority::Low;
    out.emplace_back(std::move(msg));
    return out;
}

}  // namespace app
