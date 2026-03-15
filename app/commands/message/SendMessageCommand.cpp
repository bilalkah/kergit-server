#include "app/commands/message/SendMessageCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "domains/Message.h"
#include "proto/command/message.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"
#include "proto/event/state.pb.h"

#include <cassert>
#include <string_view>
#include <vector>

namespace app {

namespace {
constexpr std::size_t kMaxMessageLength = 4096;
}

std::vector<net::outbound::OutgoingMessage> SendMessageCommand::execute(CommandContext& ctx,
                                                                        const queue::Event& evt) {
    std::vector<net::outbound::OutgoingMessage> out;
    assert(std::holds_alternative<queue::MessageEvent>(evt));
    const auto& event = std::get<queue::MessageEvent>(evt);

    const auto& env = event.payload.env;
    assert(env.type() == sercom::protocol::Envelope::MESSAGE_SEND);

    const auto& cmd = require_parsed<sercom::protocol::command::SendMessage>(event);
    const auto scope_opt = to_channel_scope(cmd.channel());
    if (!scope_opt.has_value()) {
        out.emplace_back(make_command_error(
            event.conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "channel.hub_id and channel.channel_id are required"));
        return out;
    }
    const HubId hub_id = scope_opt->hub_id;
    const ChannelId ch_id = scope_opt->channel_id;

    auto user_exp = ctx.session_manager.sessionOfConnection(event.conn_id);
    if (!user_exp.has_value()) {
        out.emplace_back(make_drop_connection(event.conn_id,
                                              sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                              "Authenticate before sending messages"));
        return out;
    }
    const UserId user_id = user_exp.value();

    if (cmd.content().empty() || cmd.content().size() > kMaxMessageLength) {
        out.emplace_back(make_command_error(
            event.conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "Message content must be between 1 and " + std::to_string(kMaxMessageLength) +
                " characters"));
        return out;
    }

    auto channel_opt = ctx.hub_service.getChannel(ch_id);
    if (!channel_opt || channel_opt->hub_id != hub_id) {
        out.emplace_back(make_command_error(event.conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_NOT_FOUND,
                                            "Channel not found"));
        return out;
    }

    if (!ctx.hub_service.isHubMember(hub_id, user_id)) {
        out.emplace_back(make_command_error(
            event.conn_id, env.type(), sercom::protocol::event::CommandErrorCode_FORBIDDEN,
            "Must be a member of the channel's hub to send messages"));
        return out;
    }

    auto saved_exp = ctx.message_service.sendMessage(ch_id, user_id, cmd.content());
    if (!saved_exp.has_value()) {
        if (saved_exp.error() == services::MessageService::MessageError::QueueFull) {
            out.emplace_back(make_command_error(
                event.conn_id, env.type(), sercom::protocol::event::CommandErrorCode_RATE_LIMITED,
                "Message queue is full, try again later"));
        } else {
            out.emplace_back(make_command_error(
                event.conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
                "Failed to send message"));
        }
        return out;
    }
    const Message saved = std::move(saved_exp.value());

    sercom::protocol::event::StateDelta delta;
    auto* hub_delta = delta.add_hubs();
    hub_delta->set_hub_id(hub_id.value);
    auto* channel_delta = hub_delta->add_channels();
    channel_delta->set_channel_id(ch_id.value);
    auto* append_op = channel_delta->add_message_ops()->mutable_append();
    *append_op->mutable_state() = to_proto_message_state(saved);

    std::vector<GlobalConnId> conns;
    utils::metrics::counters().fanout_subscriber_snapshot_total.fetch_add(
        1, std::memory_order_relaxed);
    if (auto subs = ctx.subscription_manager.getSubscribers(Topic::ChannelTopic(hub_id, ch_id))) {
        conns.reserve(subs->size());
        for (const auto& sub : *subs) {
            conns.push_back(sub);
        }
    }

    if (!conns.empty()) {
        out.emplace_back(make_outgoing_message(net::outbound::Target::many(std::move(conns)),
                                               make_state_delta(delta)));
    }
    return out;
}

}  // namespace app
