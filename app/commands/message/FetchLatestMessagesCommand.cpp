#include "app/commands/message/FetchLatestMessagesCommand.h"

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
#include <optional>
#include <vector>

namespace app {

std::vector<net::outbound::OutgoingMessage> FetchLatestMessagesCommand::execute(
    CommandContext& ctx, const queue::Event& evt) {
    std::vector<net::outbound::OutgoingMessage> out;

    assert(std::holds_alternative<queue::MessageEvent>(evt));
    const auto& event = std::get<queue::MessageEvent>(evt);
    const auto& env = event.payload.env;
    assert(env.type() == sercom::protocol::Envelope::MESSAGE_FETCH_LATEST);

    const auto& cmd = require_parsed<sercom::protocol::command::FetchLatestMessages>(event);

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

    const UserId user_id = user_exp.value();
    const HubId hub_id = channel_opt->hub_id;
    if (!ctx.hub_service.isHubMember(hub_id, user_id)) {
        out.emplace_back(make_command_error(event.conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                            "Join the hub before fetching messages"));
        return out;
    }

    const int limit = clamp_limit(cmd.limit());

    std::optional<MessageId> after_id;
    if (cmd.has_known_latest_message_id() && !cmd.known_latest_message_id().empty()) {
        after_id = MessageId{cmd.known_latest_message_id()};
    }

    std::vector<Message> messages;
    if (after_id.has_value()) {
        auto messages_exp = ctx.channel_service.fetchMessagesAfter(ch_id, *after_id, limit);
        if (!messages_exp.has_value()) {
            out.emplace_back(make_command_error(
                event.conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
                "Failed to fetch messages"));
            return out;
        }
        messages = std::move(messages_exp.value());
    } else {
        auto messages_exp = ctx.channel_service.fetchMessages(ch_id, limit);
        if (!messages_exp.has_value()) {
            out.emplace_back(make_command_error(
                event.conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
                "Failed to fetch messages"));
            return out;
        }
        messages = std::move(messages_exp.value());
        std::reverse(messages.begin(), messages.end());
    }

    std::vector<sercom::protocol::domain::Message> proto_messages;
    proto_messages.reserve(messages.size());
    for (const auto& msg : messages) {
        proto_messages.push_back(to_proto_message(msg, ctx.user_service.getUser(msg.sender_id)));
    }

    out.emplace_back(make_outgoing_message(
        net::outbound::Target::one(event.conn_id),
        make_message_batch(ch_id, sercom::protocol::event::MessageBatch::LATEST, proto_messages)));
    return out;
}

}  // namespace app
