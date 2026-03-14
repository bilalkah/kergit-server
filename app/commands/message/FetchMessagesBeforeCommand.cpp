#include "app/commands/message/FetchMessagesBeforeCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "domains/Message.h"
#include "proto/command/message.pb.h"
#include "proto/domain/message.pb.h"
#include "proto/domain/user.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"
#include "proto/event/message.pb.h"

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

    auto user_exp = ctx.session_manager.sessionOfConnection(event.conn_id);
    if (!user_exp.has_value()) {
        out.emplace_back(make_drop_connection(event.conn_id,
                                              sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                              "Must be authenticated to fetch messages"));
        return out;
    }

    const ChannelId ch_id{cmd.channel_id()};
    auto channel_opt = ctx.channel_service.getChannel(ch_id);
    if (!channel_opt.has_value()) {
        out.emplace_back(make_command_error(event.conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_NOT_FOUND,
                                            "Channel not found"));
        return out;
    }

    const HubId hub_id = channel_opt->hub_id;
    const UserId user_id = user_exp.value();
    if (!ctx.hub_service.isHubMember(hub_id, user_id)) {
        out.emplace_back(make_command_error(event.conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                            "Join the hub before fetching messages"));
        return out;
    }

    const MessageId before_internal{cmd.before_message_id()};
    const int limit = clamp_limit(cmd.limit());
    auto messages_exp = ctx.channel_service.fetchMessagesBefore(ch_id, before_internal, limit);
    if (!messages_exp.has_value()) {
        out.emplace_back(make_command_error(
            event.conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
            "Failed to fetch messages"));
        return out;
    }
    std::vector<Message> messages = std::move(messages_exp.value());
    std::reverse(messages.begin(), messages.end());

    std::vector<sercom::protocol::domain::Message> proto_messages;
    proto_messages.reserve(messages.size());
    for (const auto& msg : messages) {
        proto_messages.push_back(to_proto_message(msg, ctx.user_service.getUser(msg.sender_id)));
    }

    out.emplace_back(make_outgoing_message(
        net::outbound::Target::one(event.conn_id),
        make_message_batch(ch_id, sercom::protocol::event::MessageBatch::BEFORE, proto_messages)));
    return out;
}

}  // namespace app
