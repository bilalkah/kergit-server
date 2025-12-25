#ifndef DOMAINS_MESSAGE_H
#define DOMAINS_MESSAGE_H

#include "domains/ids/Ids.h"

#include <chrono>
#include <string>
#include <vector>

enum class MessageType {
    CHAT,
    SYSTEM,
};

enum class MessageStatus { PENDING, SENT, DELIVERED, READ, FAILED, BLOCKED };

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
    std::chrono::system_clock::time_point sent_at{};
};

#endif  // DOMAINS_MESSAGE_H
