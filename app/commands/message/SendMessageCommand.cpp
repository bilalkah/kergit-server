#include "app/commands/message/SendMessageCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "app/proto_builders/EnvelopeBuilders.h"
#include "domains/Message.h"
#include "proto/command/message.pb.h"
#include "proto/domain/message.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"
#include "proto/event/message.pb.h"
#include "utils/EventLogger.h"

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
    out.set_id(msg.id.value);
    out.set_author_id(msg.sender_id.value);
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

    const auto& cmd = require_parsed<sercom::protocol::command::SendMessage>(*event);

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!user_exp.has_value()) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
            "Authenticate first"));
    }
    const UserId user_id = user_exp.value();

    if (cmd.content().empty()) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "Message content cannot be empty"));
    }
    if (cmd.content().size() > kMaxMessageLength) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "Message exceeds maximum length"));
    }

    auto channel_id_opt = parse_wire_id<ChannelId>(cmd.channel_id());
    if (!channel_id_opt.has_value()) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_NOT_FOUND,
            "Channel not found"));
    }

    auto channel_opt = ctx.channel_service.getChannel(*channel_id_opt);
    if (!channel_opt) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_NOT_FOUND,
            "Channel not found"));
    }
    const HubId hub_id = channel_opt->hub_id;

    if (!ctx.hub_service.isHubMember(hub_id, user_id)) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_FORBIDDEN,
            "Join the hub before sending messages"));
    }

    if (!connection_has_channel_subscription(ctx.subscription_manager, event->conn_id, hub_id,
                                             *channel_id_opt)) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "Channel is not active"));
    }

    // Time the DB/message save operation
    auto db_start = std::chrono::steady_clock::now();
    auto saved_exp = ctx.channel_service.sendMessage(*channel_id_opt, user_id, cmd.content());
    auto db_end = std::chrono::steady_clock::now();
    auto db_duration_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(db_end - db_start).count();

    if (!saved_exp.has_value()) {
        if (saved_exp.error() == services::ChannelService::MessageError::QueueFull) {
            return single_outgoing(make_command_error(
                event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_RATE_LIMITED,
                "Server is busy, try again"));
        }
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
            "Failed to send message"));
    }
    Message saved = std::move(saved_exp.value());

    // Log message sent event with DB timing
    utils::EventLogger::instance().message_sent(user_id.value, channel_id_opt->value,
                                                db_duration_ms);

    sercom::protocol::event::MessageCreated created;
    created.set_channel_id(channel_id_opt->value);
    *created.mutable_message() = to_proto_message(ctx, saved);

    std::string bytes =
        proto_builders::serialize_envelope(sercom::protocol::Envelope::MESSAGE_CREATED, created);

    std::vector<GlobalConnId> conns;
    utils::metrics::counters().fanout_subscriber_snapshot_total.fetch_add(
        1, std::memory_order_relaxed);
    auto subs = ctx.subscription_manager.getSubscribers(Topic::ChannelTopic(hub_id, *channel_id_opt));
    if (subs) {
        for (const auto& conn : *subs) {
            conns.push_back(conn);
        }
    }

    if (conns.empty()) {
        return {};
    }

    return single_outgoing(net::outbound::OutgoingMessage{
        .target = net::outbound::Target::many(std::move(conns)),
        .action =
            net::outbound::Action{std::in_place_type<net::outbound::SendPayload>,
                                  net::outbound::SendPayload{
                                      .payload = net::outbound::Payload{std::move(bytes), true}}}});
}

}  // namespace app
