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
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace app {

namespace {
constexpr std::size_t kMaxMessageLength = 4096;
constexpr std::size_t kMaxAttachmentCount = 6;
constexpr std::size_t kMaxAttachmentDisplayNameLength = 255;
constexpr std::size_t kMaxAttachmentMimeTypeLength = 127;
constexpr std::size_t kMaxAttachmentStorageKeyLength = 1024;
constexpr std::size_t kMaxAttachmentStorageBucketLength = 80;
constexpr std::uint64_t kMaxAttachmentSizeBytes = 15ULL * 1024ULL * 1024ULL;
constexpr std::size_t kMaxLinkPreviewFieldLength = 2048;

bool is_http_or_https(const std::string& value) {
    return value.starts_with("https://") || value.starts_with("http://");
}

std::optional<std::vector<MessageAttachment>> parse_attachments(
    const sercom::protocol::command::SendMessage& cmd, const std::string& expected_storage_prefix) {
    std::vector<MessageAttachment> attachments;
    attachments.reserve(cmd.attachments_size());
    for (const auto& input : cmd.attachments()) {
        if (input.id().empty() || input.storage_key().empty() || input.storage_bucket().empty() ||
            input.display_name().empty()) {
            return std::nullopt;
        }
        if (!expected_storage_prefix.empty() &&
            input.storage_key().rfind(expected_storage_prefix, 0) != 0) {
            return std::nullopt;
        }
        if (input.display_name().size() > kMaxAttachmentDisplayNameLength ||
            input.mime_type().size() > kMaxAttachmentMimeTypeLength ||
            input.storage_key().size() > kMaxAttachmentStorageKeyLength ||
            input.storage_bucket().size() > kMaxAttachmentStorageBucketLength) {
            return std::nullopt;
        }
        if (input.size_bytes() == 0 || input.size_bytes() > kMaxAttachmentSizeBytes) {
            return std::nullopt;
        }
        MessageAttachment next;
        next.id = input.id();
        next.kind =
            input.kind() ==
                    sercom::protocol::domain::MessageAttachmentKind::MessageAttachmentKind_IMAGE
                ? MessageAttachmentKind::IMAGE
                : MessageAttachmentKind::FILE;
        next.storage_bucket = input.storage_bucket();
        next.storage_key = input.storage_key();
        next.mime_type = input.mime_type();
        next.display_name = input.display_name();
        next.size_bytes = input.size_bytes();
        attachments.push_back(std::move(next));
    }
    return attachments;
}

std::optional<MessageLinkPreview> parse_link_preview(
    const sercom::protocol::command::SendMessage& cmd) {
    if (!cmd.has_link_preview()) return std::nullopt;
    const auto& raw = cmd.link_preview();
    if (raw.url().empty()) return std::nullopt;
    if (!is_http_or_https(raw.url())) return std::nullopt;
    if (!raw.image_url().empty() && !is_http_or_https(raw.image_url())) return std::nullopt;
    if (raw.url().size() > kMaxLinkPreviewFieldLength ||
        raw.title().size() > kMaxLinkPreviewFieldLength ||
        raw.description().size() > kMaxLinkPreviewFieldLength ||
        raw.site_name().size() > kMaxLinkPreviewFieldLength ||
        raw.image_url().size() > kMaxLinkPreviewFieldLength) {
        return std::nullopt;
    }
    MessageLinkPreview preview;
    preview.url = raw.url();
    preview.title = raw.title();
    preview.description = raw.description();
    preview.site_name = raw.site_name();
    preview.image_url = raw.image_url();
    return preview;
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

    if (cmd.content().size() > kMaxMessageLength) {
        out.emplace_back(make_command_error(
            event.conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "Message content must be between 0 and " + std::to_string(kMaxMessageLength) +
                " characters"));
        return out;
    }

    if (cmd.attachments_size() < 0 ||
        static_cast<std::size_t>(cmd.attachments_size()) > kMaxAttachmentCount) {
        out.emplace_back(make_command_error(
            event.conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "Too many attachments"));
        return out;
    }

    const std::string attachment_prefix =
        hub_id.value + "/" + ch_id.value + "/" + user_id.value + "/";
    const auto attachments_opt = parse_attachments(cmd, attachment_prefix);
    if (!attachments_opt.has_value()) {
        out.emplace_back(make_command_error(
            event.conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "Invalid attachment payload"));
        return out;
    }
    const auto link_preview_opt = parse_link_preview(cmd);
    if (cmd.has_link_preview() && !link_preview_opt.has_value()) {
        out.emplace_back(make_command_error(
            event.conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "Invalid link preview payload"));
        return out;
    }

    if (cmd.content().empty() && attachments_opt->empty()) {
        out.emplace_back(make_command_error(
            event.conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "Message content or attachments are required"));
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

    auto saved_exp = ctx.message_service.sendMessage(ch_id, user_id, cmd.content(),
                                                     *attachments_opt, link_preview_opt);
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
