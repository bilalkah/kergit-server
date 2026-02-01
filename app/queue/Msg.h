#ifndef APP_QUEUE_MSG_H
#define APP_QUEUE_MSG_H

#include "domains/ids/Ids.h"
#include "proto/ParsedPayload.h"
#include "proto/envelope.pb.h"

#include <cstdint>
#include <string>
#include <type_traits>
#include <variant>

namespace app::queue {

struct Payload {
    sercom::protocol::Envelope env;
    sercom::protocol::ParsedPayload parsed{};
};

struct ConnectionEvent {
    GlobalConnId conn_id;
    UserId user_id;
};

struct DisconnectionEvent {
    GlobalConnId conn_id;
    int code{};
    std::string reason;
};

struct MessageEvent {
    GlobalConnId conn_id;
    Payload payload;
};

enum class EventPriority : uint8_t { High = 0, Low = 1 };

enum class PushResult {
    Ok,
    DroppedLow,
    DroppedHigh,
};

using Event = std::variant<ConnectionEvent, DisconnectionEvent, MessageEvent>;

inline EventPriority classify_event(const Event& evt) {
    return std::visit(
        [](auto&& ev) -> EventPriority {
            using T = std::decay_t<decltype(ev)>;
            if constexpr (std::is_same_v<T, MessageEvent>) {
                switch (ev.payload.env.type()) {
                    // Low priority: UI hints only.
                    case sercom::protocol::Envelope::TYPING:
                    case sercom::protocol::Envelope::PRESENCE:
                        return EventPriority::Low;
                    // Voice activity is low priority ONLY if it never encodes join/leave.
                    // Membership truth must remain high priority under overload.
                    case sercom::protocol::Envelope::VOICE_ACTIVITY:
                        return EventPriority::Low;
                    // Voice membership / presence updates are authoritative.
                    case sercom::protocol::Envelope::VOICE_JOIN:
                    case sercom::protocol::Envelope::VOICE_CHANNEL_PARTICIPANTS:
                    case sercom::protocol::Envelope::VOICE_CHANNEL_PRESENCE:
                        return EventPriority::High;
                    default:
                        return EventPriority::High;
                }
            }
            return EventPriority::High;
        },
        evt);
}

}  // namespace app::queue

#endif  // APP_QUEUE_MSG_H
