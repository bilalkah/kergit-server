#ifndef DOMAINS_MESSAGE_H
#define DOMAINS_MESSAGE_H

#include "domains/ids/Ids.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

enum class MessageType {
    CHAT,
    SYSTEM,
};

enum class MessageStatus { PENDING, SENT, DELIVERED, READ, FAILED, BLOCKED };

enum class MessageAttachmentKind {
    IMAGE,
    FILE,
};

struct MessageAttachment {
    std::string id{};
    MessageAttachmentKind kind{MessageAttachmentKind::FILE};
    std::string storage_bucket{};
    std::string storage_key{};
    std::string mime_type{};
    std::string display_name{};
    uint64_t size_bytes{0};
};

struct MessageLinkPreview {
    std::string url{};
    std::string title{};
    std::string description{};
    std::string site_name{};
    std::string image_url{};
};

struct MessageCursor {
    // Channel-local monotonic sequence used for ordering and pagination.
    uint64_t message_seq{0};
    // Retained for display/backward-compatibility only; not used for pagination.
    MessageId message_id{""};
    uint64_t created_at_unix_us{0};
};

struct Message {
    Message() {}
    Message(MessageId mid, ChannelId ch, UserId sender, std::string body,
            MessageType mt = MessageType::CHAT)
        : id(std::move(mid)),
          ch_id(std::move(ch)),
          sender_id(std::move(sender)),
          type(mt),
          text(std::move(body)) {}

    MessageId id{""};
    ChannelId ch_id{""};
    UserId sender_id{""};

    // Content
    MessageType type{MessageType::CHAT};
    std::string text{""};  // keep plain; validate in services
    std::vector<MessageAttachment> attachments{};
    std::optional<MessageLinkPreview> link_preview{};

    MessageStatus status{MessageStatus::PENDING};

    // Channel-local monotonic sequence assigned by the DB (0 until persisted).
    uint64_t message_seq{0};

    // Timestamps
    uint64_t created_at_unix_us{0};
};

#endif  // DOMAINS_MESSAGE_H
