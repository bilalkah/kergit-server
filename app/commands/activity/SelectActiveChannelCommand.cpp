#include "app/commands/activity/SelectActiveChannelCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "domains/Message.h"
#include "proto/command/activity.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"
#include "proto/event/state.pb.h"

#include <algorithm>
#include <cassert>
#include <optional>
#include <vector>

namespace app {

std::vector<net::outbound::OutgoingMessage> SelectActiveChannelCommand::execute(
    CommandContext& ctx, const queue::Event& evt) {
    std::vector<net::outbound::OutgoingMessage> out;

    assert(std::holds_alternative<queue::MessageEvent>(evt));
    const auto* event = &std::get<queue::MessageEvent>(evt);
    const auto& env = event->payload.env;
    assert(env.type() == sercom::protocol::Envelope::ACTIVE_CHANNEL);

    const auto& cmd = require_parsed<sercom::protocol::command::SelectActiveChannel>(*event);
    const auto scope_opt = to_channel_scope(cmd.channel());
    if (!scope_opt.has_value()) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(),
            sercom::protocol::event::CommandErrorCode::CommandErrorCode_INVALID_ARGUMENT,
            "channel.hub_id and channel.channel_id are required"));
        return out;
    }
    const HubId hub_id = scope_opt->hub_id;
    const ChannelId channel_id = scope_opt->channel_id;

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!user_exp.has_value()) {
        out.emplace_back(make_drop_connection(
            event->conn_id, sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
            "Unauthorized: No session associated with this connection"));
        return out;
    }
    const UserId user_id = user_exp.value();

    auto channel_opt = ctx.hub_service.getChannel(channel_id);
    if (!channel_opt || channel_opt->hub_id != hub_id) {
        out.emplace_back(make_command_error(event->conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_NOT_FOUND,
                                            "Channel not found"));
        return out;
    }

    // ACTIVE_CHANNEL drives text-message focus/fetch; voice channels are joined
    // through the voice flow, not selected here.
    if (channel_opt->type != ChannelType::CHAT) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "Channel is not a text channel"));
        return out;
    }

    if (!ctx.hub_service.isHubMember(hub_id, user_id)) {
        out.emplace_back(make_command_error(event->conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                            "Join the hub before selecting a channel"));
        return out;
    }

    const auto channel_topic = Topic::ChannelTopic(hub_id, channel_id);
    ctx.subscription_manager.subscribeConnection(event->conn_id, channel_topic);
    ctx.session_manager.joinTextChannel(user_id, hub_id, channel_id);

    const int latest_limit = clamp_limit(cmd.latest_limit());
    std::optional<MessageCursor> known_latest;
    if (cmd.has_known_latest_cursor()) {
        const auto& cursor = cmd.known_latest_cursor();
        if (cursor.message_seq() == 0) {
            out.emplace_back(make_command_error(
                event->conn_id, env.type(),
                sercom::protocol::event::CommandErrorCode::CommandErrorCode_INVALID_ARGUMENT,
                "known_latest_cursor.message_seq is required"));
            return out;
        }
        known_latest = MessageCursor{
            .message_seq = cursor.message_seq(),
        };
    }

    std::vector<Message> messages;
    if (known_latest.has_value()) {
        auto page = ctx.message_service.fetchMessagesAfter(channel_id, *known_latest, latest_limit);
        if (!page.has_value()) {
            out.emplace_back(
                make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
                                   "Failed to fetch latest messages"));
            return out;
        }
        messages = std::move(page.value());
        // Background catch-up is a no-op when there are no new messages.
        if (messages.empty()) {
            return out;
        }
    } else {
        auto page = ctx.message_service.fetchMessages(channel_id, latest_limit);
        if (!page.has_value()) {
            out.emplace_back(
                make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
                                   "Failed to fetch latest messages"));
            return out;
        }
        messages = std::move(page.value());
        std::reverse(messages.begin(), messages.end());
    }

    sercom::protocol::event::StateDelta delta;
    auto* hub_delta = delta.add_hubs();
    hub_delta->set_hub_id(hub_id.value);
    auto* channel_delta = hub_delta->add_channels();
    channel_delta->set_channel_id(channel_id.value);
    auto* batch = channel_delta->add_message_ops()->mutable_batch();
    batch->set_direction(sercom::protocol::event::MessageBatch::LATEST);
    for (const auto& msg : messages) {
        *batch->add_states() = to_proto_message_state(msg);
    }

    out.emplace_back(
        make_outgoing_message(net::outbound::Target::one(event->conn_id), make_state_delta(delta)));
    return out;
}

}  // namespace app
