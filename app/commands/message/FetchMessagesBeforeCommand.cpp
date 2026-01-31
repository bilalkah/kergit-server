#include "app/commands/message/FetchMessagesBeforeCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "domains/Message.h"
#include "proto/command/message.pb.h"
#include "proto/domain/message.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"
#include "proto/event/message.pb.h"

#include <algorithm>
#include <chrono>
#include <string_view>
#include <vector>

namespace app {

namespace {
constexpr int kDefaultLimit = 50;
constexpr int kMaxLimit = 100;

int clamp_limit(uint32_t limit) {
    if (limit == 0) return kDefaultLimit;
    return std::min(static_cast<int>(limit), kMaxLimit);
}

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

std::vector<net::outbound::OutgoingMessage> FetchMessagesBeforeCommand::execute(
    CommandContext& ctx, const queue::Event& evt) {
    const auto* event = std::get_if<queue::MessageEvent>(&evt);
    if (!event) {
        return {};
    }

    const auto& env = event->payload.env;
    if (env.type() != sercom::protocol::Envelope::MESSAGE_FETCH_BEFORE) {
        return {};
    }

    sercom::protocol::command::FetchMessagesBefore cmd;
    if (!cmd.ParseFromString(env.payload())) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INVALID_FORMAT,
                                   "Invalid MESSAGE_FETCH_BEFORE payload")};
    }

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!user_exp.has_value()) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
                                   "Authenticate first")};
    }
    const UserId user_id = user_exp.value();

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
                                   "Join the hub before fetching messages")};
    }

    auto before_internal = ctx.ids.to_internal(PublicMessageId{cmd.before_message_id()});
    if (!before_internal.has_value()) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_NOT_FOUND,
                                   "Message not found")};
    }

    const int limit = clamp_limit(cmd.limit());
    auto messages_exp =
        ctx.channel_service.fetchMessagesBefore(*channel_id_opt, *before_internal, limit);
    if (!messages_exp.has_value()) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
                                   "Failed to fetch messages")};
    }
    std::vector<Message> messages = std::move(messages_exp.value());
    std::reverse(messages.begin(), messages.end());

    sercom::protocol::event::MessageBatch batch;
    batch.set_hub_id(ctx.ids.to_public(*hub_id_opt).value);
    batch.set_channel_id(ctx.ids.to_public(*channel_id_opt).value);
    batch.set_direction(sercom::protocol::event::MessageBatch::BEFORE);

    for (const auto& msg : messages) {
        *batch.add_messages() = to_proto_message(ctx, msg);
    }

    sercom::protocol::Envelope out_env;
    out_env.set_version(1);
    out_env.set_type(sercom::protocol::Envelope::MESSAGE_BATCH);
    batch.SerializeToString(out_env.mutable_payload());

    std::string bytes;
    out_env.SerializeToString(&bytes);

    return {net::outbound::OutgoingMessage{
        .target = net::outbound::Target::one(event->conn_id),
        .action = net::outbound::SendPayload{
            .payload = net::outbound::Payload{.data = std::move(bytes), .is_binary = true}}}};
}

}  // namespace app
