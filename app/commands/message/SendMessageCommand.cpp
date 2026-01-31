#include "app/commands/message/SendMessageCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "domains/Message.h"
#include "proto/command/message.pb.h"
#include "proto/domain/message.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"
#include "proto/event/message.pb.h"

#include <chrono>
#include <string_view>
#include <vector>

namespace app {

namespace {
constexpr std::size_t kMaxMessageLength = 4096;

uint64_t to_epoch_ms(const std::chrono::system_clock::time_point& tp) {
    if (tp.time_since_epoch().count() == 0) return 0;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
    return ms > 0 ? static_cast<uint64_t>(ms) : 0;
}

sercom::protocol::domain::Message to_proto_message(CommandContext& ctx, const Message& msg) {
    sercom::protocol::domain::Message out;
    out.set_id(ctx.ids.to_public(msg.id).value);
    out.set_author_id(ctx.ids.to_public(msg.sender_id).value);
    out.set_content(msg.text);
    out.set_created_at_ms(to_epoch_ms(msg.sent_at));
    return out;
}

}  // namespace

std::vector<net::outbound::OutgoingMessage> SendMessageCommand::execute(CommandContext& ctx,
                                                                        const queue::Event& evt) {
    const auto* event = std::get_if<queue::MessageEvent>(&evt);
    if (!event) {
        return {};
    }

    const auto& env = event->payload.env;
    if (env.type() != sercom::protocol::Envelope::MESSAGE_SEND) {
        return {};
    }

    sercom::protocol::command::SendMessage cmd;
    if (!cmd.ParseFromString(env.payload())) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INVALID_FORMAT,
                                   "Invalid MESSAGE_SEND payload")};
    }

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!user_exp.has_value()) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
                                   "Authenticate first")};
    }
    const UserId user_id = user_exp.value();

    if (cmd.content().empty()) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                                   "Message content cannot be empty")};
    }
    if (cmd.content().size() > kMaxMessageLength) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                                   "Message exceeds maximum length")};
    }

    auto hub_id_opt = ctx.ids.to_internal(PublicHubId{cmd.hub_id()});
    if (!hub_id_opt.has_value()) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_NOT_FOUND,
                                   "Hub not found")};
    }

    auto channel_id_opt = ctx.ids.to_internal(PublicChannelId{cmd.channel_id()});
    if (!channel_id_opt.has_value()) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_NOT_FOUND,
                                   "Channel not found")};
    }

    auto channel_opt = ctx.channel_service.getChannel(*channel_id_opt);
    if (!channel_opt.has_value() || channel_opt->hub_id != *hub_id_opt) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_NOT_FOUND,
                                   "Channel not found")};
    }

    if (!ctx.hub_service.isHubMember(*hub_id_opt, user_id)) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                   "Join the hub before sending messages")};
    }

    auto session = ctx.session_manager.getSession(user_id);
    if (!session.has_value() || !session->current_text_channel ||
        session->current_text_channel.value() != *channel_id_opt) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                                   "Channel is not active")};
    }

    auto saved_exp =
        ctx.channel_service.sendMessage(*channel_id_opt, user_id, cmd.content());
    if (!saved_exp.has_value()) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
                                   "Failed to send message")};
    }
    Message saved = std::move(saved_exp.value());

    sercom::protocol::event::MessageCreated created;
    created.set_hub_id(ctx.ids.to_public(*hub_id_opt).value);
    created.set_channel_id(ctx.ids.to_public(*channel_id_opt).value);
    *created.mutable_message() = to_proto_message(ctx, saved);

    sercom::protocol::Envelope out_env;
    out_env.set_version(1);
    out_env.set_type(sercom::protocol::Envelope::MESSAGE_CREATED);
    created.SerializeToString(out_env.mutable_payload());

    std::string bytes;
    out_env.SerializeToString(&bytes);

    std::vector<GlobalConnId> conns;
    auto subs =
        ctx.subscription_manager.getSubscribers(Topic::ChannelTopic(*hub_id_opt, *channel_id_opt));
    if (subs.has_value()) {
        for (const auto& uid : subs.value()) {
            auto conn = ctx.session_manager.getMainConnection(uid);
            if (conn.has_value()) {
                conns.push_back(conn.value());
            }
        }
    }

    if (conns.empty()) {
        return {};
    }

    return {net::outbound::OutgoingMessage{
        .target = net::outbound::Target::many(std::move(conns)),
        .action = net::outbound::SendPayload{
            .payload = net::outbound::Payload{.data = std::move(bytes), .is_binary = true}}}};
}

}  // namespace app
