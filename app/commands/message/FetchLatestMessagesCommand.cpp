#include "app/commands/message/FetchLatestMessagesCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/proto_builders/EnvelopeBuilders.h"
#include "domains/Message.h"
#include "proto/command/message.pb.h"
#include "proto/domain/message.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"
#include "proto/event/message.pb.h"

#include <algorithm>
#include <chrono>
#include <optional>
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

sercom::protocol::domain::Message to_proto_message(const Message& msg) {
    sercom::protocol::domain::Message out;
    out.set_id(msg.id.value);
    out.set_author_id(msg.sender_id.value);
    out.set_content(msg.text);
    out.set_created_at_ms(to_epoch_ms(msg.sent_at));
    return out;
}
}  // namespace

std::vector<net::outbound::OutgoingMessage> FetchLatestMessagesCommand::execute(
    CommandContext& ctx, const queue::Event& evt) {
    const auto* event = std::get_if<queue::MessageEvent>(&evt);
    if (!event) {
        return {};
    }

    const auto& env = event->payload.env;
    if (env.type() != sercom::protocol::Envelope::MESSAGE_FETCH_LATEST) {
        return {};
    }

    const auto& cmd = require_parsed<sercom::protocol::command::FetchLatestMessages>(*event);

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!user_exp.has_value()) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
            "Authenticate first"));
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
            "Join the hub before fetching messages"));
    }

    const int limit = clamp_limit(cmd.limit());

    std::optional<MessageId> after_id;
    if (cmd.has_known_latest_message_id() && !cmd.known_latest_message_id().empty()) {
        after_id = MessageId{cmd.known_latest_message_id()};
    }

    std::vector<Message> messages;
    if (after_id.has_value()) {
        auto messages_exp =
            ctx.channel_service.fetchMessagesAfter(*channel_id_opt, *after_id, limit);
        if (!messages_exp.has_value()) {
            return single_outgoing(
                make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
                                   "Failed to fetch messages"));
        }
        messages = std::move(messages_exp.value());
    } else {
        auto messages_exp = ctx.channel_service.fetchMessages(*channel_id_opt, limit);
        if (!messages_exp.has_value()) {
            return single_outgoing(
                make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
                                   "Failed to fetch messages"));
        }
        messages = std::move(messages_exp.value());
        std::reverse(messages.begin(), messages.end());
    }

    sercom::protocol::event::MessageBatch batch;
    batch.set_channel_id(channel_id_opt->value);
    batch.set_direction(sercom::protocol::event::MessageBatch::LATEST);

    for (const auto& msg : messages) {
        *batch.add_messages() = to_proto_message(msg);
    }

    std::string bytes =
        proto_builders::serialize_envelope(sercom::protocol::Envelope::MESSAGE_BATCH, batch);

    return single_outgoing(net::outbound::OutgoingMessage{
        .target = net::outbound::Target::one(event->conn_id),
        .action =
            net::outbound::Action{std::in_place_type<net::outbound::SendPayload>,
                                  net::outbound::SendPayload{
                                      .payload = net::outbound::Payload{std::move(bytes), true}}}});
}

}  // namespace app
