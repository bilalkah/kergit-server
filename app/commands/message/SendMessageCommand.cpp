#include "app/commands/message/SendMessageCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "domains/Message.h"
#include "proto/command/message.pb.h"
#include "proto/domain/message.pb.h"
#include "proto/domain/user.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"
#include "proto/event/message.pb.h"

#include <cassert>
#include <string_view>
#include <vector>

namespace app {

namespace {
constexpr std::size_t kMaxMessageLength = 4096;

std::string make_message_created(CommandContext& ctx, const ChannelId& channel_id,
                                 const Message& message) {
    sercom::protocol::event::MessageCreated created;
    created.set_channel_id(channel_id.value);

    auto* proto_message = created.mutable_message();
    proto_message->set_id(message.id.value);
    proto_message->set_author_id(message.sender_id.value);
    proto_message->set_content(message.text);
    proto_message->set_created_at_ms(to_epoch_ms(message.sent_at));

    auto author_opt = ctx.user_service.getUser(message.sender_id);
    if (author_opt.has_value()) {
        auto* author = proto_message->mutable_author();
        author->set_id(author_opt->id.value);
        author->mutable_metadata()->set_username(author_opt->username);
        author->mutable_metadata()->set_avatar_seed(author_opt->avatar_seed);
    }

    sercom::protocol::Envelope out_env;
    out_env.set_version(1);
    out_env.set_type(sercom::protocol::Envelope::MESSAGE_CREATED);
    created.SerializeToString(out_env.mutable_payload());
    return out_env.SerializeAsString();
}

}  // namespace

std::vector<net::outbound::OutgoingMessage> SendMessageCommand::execute(CommandContext& ctx,
                                                                        const queue::Event& evt) {
    std::vector<net::outbound::OutgoingMessage> out;
    assert(std::holds_alternative<queue::MessageEvent>(evt));
    const auto& event = std::get<queue::MessageEvent>(evt);

    const auto& env = event.payload.env;
    assert(env.type() == sercom::protocol::Envelope::MESSAGE_SEND);

    const auto& cmd = require_parsed<sercom::protocol::command::SendMessage>(event);

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

    const ChannelId ch_id{cmd.channel_id()};
    auto channel_opt = ctx.channel_service.getChannel(ch_id);
    if (!channel_opt) {
        out.emplace_back(make_command_error(event.conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_NOT_FOUND,
                                            "Channel not found"));
        return out;
    }
    const HubId hub_id = channel_opt->hub_id;

    if (!ctx.hub_service.isHubMember(hub_id, user_id)) {
        out.emplace_back(make_command_error(
            event.conn_id, env.type(), sercom::protocol::event::CommandErrorCode_FORBIDDEN,
            "Must be a member of the channel's hub to send messages"));
        return out;
    }

    auto saved_exp = ctx.channel_service.sendMessage(ch_id, user_id, cmd.content());

    if (!saved_exp.has_value()) {
        if (saved_exp.error() == services::ChannelService::MessageError::QueueFull) {
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
    Message saved = std::move(saved_exp.value());

    std::string bytes = make_message_created(ctx, ch_id, saved);

    std::vector<GlobalConnId> conns;
    auto subs = ctx.subscription_manager.getSubscribers(Topic::ChannelTopic(hub_id, ch_id));
    conns.reserve(subs->size());
    for (const auto& sub : *subs) {
        conns.push_back(sub);
    }
    out.emplace_back(
        make_outgoing_message(net::outbound::Target::many(std::move(conns)), std::move(bytes)));

    return out;
}

}  // namespace app
