#ifndef DOMAINS_MESSAGE_H
#define DOMAINS_MESSAGE_H

#include "domains/ids/Ids.h"

#include <chrono>
#include <string>
#include <vector>

enum class MessageType {
    CHAT,
    SYSTEM,
    JOIN_CHANNEL,
    LEAVE_CHANNEL,
    USER_STATE_CHANGE,
    USER_TYPING,
    USER_STOP_TYPING
};

enum class MessageStatus { PENDING, SENT, DELIVERED, READ, FAILED, BLOCKED };

struct Message {
    // Identifiers
    MessageId id{""};
    ChannelId channel_id{""};
    UserId sender_id{""};

    // Content
    MessageType type{MessageType::CHAT};
    std::string text{};             // keep plain; validate in services
    std::string sender_username{};  // optional display hint

    // Threading / relations (optional)
    MessageId reply_to{""};  // empty means no reply
    std::vector<std::string> attachment_ids{};

    // State
    MessageStatus status{MessageStatus::PENDING};
    int64_t sequence_number{0};  // per-channel ordering if you use it

    // Timestamps
    std::chrono::system_clock::time_point sent_at{};
    std::chrono::system_clock::time_point edited_at{};
    bool is_edited{false};
    bool is_deleted{false};

    // Ctors
    Message() = default;  // lets you use map[key] safely
    Message(MessageId mid, ChannelId ch, UserId sender, std::string body,
            MessageType mt = MessageType::CHAT)
        : id(std::move(mid)),
          channel_id(std::move(ch)),
          sender_id(std::move(sender)),
          type(mt),
          text(std::move(body)) {}

    // Tiny helpers (pure, no I/O)
    bool is_chat() const noexcept { return type == MessageType::CHAT; }
    bool is_system() const noexcept { return type == MessageType::SYSTEM; }
};

#endif  // DOMAINS_MESSAGE_H
