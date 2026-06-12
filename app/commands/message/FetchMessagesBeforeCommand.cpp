#include "app/commands/message/FetchMessagesBeforeCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "domains/Message.h"
#include "proto/command/message.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"
#include "proto/event/state.pb.h"

#include <algorithm>
#include <cassert>
#include <vector>

namespace app {

std::vector<net::outbound::OutgoingMessage> FetchMessagesBeforeCommand::execute(
    CommandContext& ctx, const queue::Event& evt) {
    std::vector<net::outbound::OutgoingMessage> out;

    assert(std::holds_alternative<queue::MessageEvent>(evt));
    const auto& event = std::get<queue::MessageEvent>(evt);
    const auto& env = event.payload.env;
    assert(env.type() == sercom::protocol::Envelope::MESSAGE_FETCH_BEFORE);

    const auto& cmd = require_parsed<sercom::protocol::command::FetchMessagesBefore>(event);
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
                                              "Must be authenticated to fetch messages"));
        return out;
    }

    auto channel_opt = ctx.hub_service.getChannel(ch_id);
    if (!channel_opt || channel_opt->hub_id != hub_id) {
        out.emplace_back(make_command_error(event.conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_NOT_FOUND,
                                            "Channel not found"));
        return out;
    }

    const UserId user_id = user_exp.value();
    if (!ctx.hub_service.isHubMember(hub_id, user_id)) {
        out.emplace_back(make_command_error(event.conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                            "Join the hub before fetching messages"));
        return out;
    }

    if (!cmd.has_before_cursor() || cmd.before_cursor().message_id().empty() ||
        cmd.before_cursor().created_at_unix_us() == 0) {
        out.emplace_back(make_command_error(
            event.conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "before_cursor.message_id and before_cursor.created_at_unix_us are required"));
        return out;
    }
    const MessageCursor before_internal{
        .message_id = MessageId{cmd.before_cursor().message_id()},
        .created_at_unix_us = cmd.before_cursor().created_at_unix_us(),
    };
    const int limit = clamp_limit(cmd.limit());
    auto messages_exp = ctx.message_service.fetchMessagesBefore(ch_id, before_internal, limit);
    if (!messages_exp.has_value()) {
        out.emplace_back(make_command_error(
            event.conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
            "Failed to fetch messages"));
        return out;
    }
    std::vector<Message> messages = std::move(messages_exp.value());
    std::reverse(messages.begin(), messages.end());

    sercom::protocol::event::StateDelta delta;
    auto* hub_delta = delta.add_hubs();
    hub_delta->set_hub_id(hub_id.value);
    auto* channel_delta = hub_delta->add_channels();
    channel_delta->set_channel_id(ch_id.value);
    auto* batch = channel_delta->add_message_ops()->mutable_batch();
    batch->set_direction(sercom::protocol::event::MessageBatch::BEFORE);
    batch->set_exhausted_before(messages.size() < static_cast<size_t>(limit));
    for (const auto& msg : messages) {
        *batch->add_states() = to_proto_message_state(msg);
    }

    out.emplace_back(make_outgoing_message(net::outbound::Target::one(event.conn_id),
                                           make_state_delta(delta)));
    return out;
}

}  // namespace app
