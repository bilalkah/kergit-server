#ifndef DOMAINS_MESSAGE_H
#define DOMAINS_MESSAGE_H

#include "domains/ids/Ids.h"

#include <cstdint>
#include <string>
#include <vector>

enum class MessageType {
    CHAT,
    SYSTEM,
};

enum class MessageStatus { PENDING, SENT, DELIVERED, READ, FAILED, BLOCKED };

struct MessageCursor {
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

    MessageStatus status{MessageStatus::PENDING};

    // Timestamps
    uint64_t created_at_unix_us{0};
};

#endif  // DOMAINS_MESSAGE_H
