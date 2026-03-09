#ifndef LIVEKIT_WEBHOOK_LIVEKITEVENT_H
#define LIVEKIT_WEBHOOK_LIVEKITEVENT_H

#include <string>
#include <cstdint>
#include <iostream>

// Domain identifiers (use your existing domain types)
#include "domains/ids/Ids.h"

namespace livekit::webhook {

// Minimal set of LiveKit webhook events used by the system
enum class LiveKitEventType : uint8_t {
    UNKNOWN = 0,

    ROOM_STARTED,
    ROOM_FINISHED,

    PARTICIPANT_JOINED,
    PARTICIPANT_LEFT,
};

// Normalized event forwarded to the application layer
struct LiveKitEvent {
    LiveKitEventType type = LiveKitEventType::UNKNOWN;

    // LiveKit room corresponds to our voice channel
    ChannelId channel_id;

    // User identity from LiveKit participant identity
    UserId user_id;

    // The LiveKit node that owns the room (e.g. "ND_xxxx")
    std::string node_id;

    uint64_t timestamp_ms = 0;

    friend std::ostream& operator<<(std::ostream& os, const LiveKitEvent& e) {
        os << "LiveKitEvent{type=" << static_cast<int>(e.type) << ", channel_id=" << e.channel_id.value
           << ", user_id=" << e.user_id.value << ", node_id=" << e.node_id
           << ", timestamp_ms=" << e.timestamp_ms << "}";
        return os;
    }
};

// Utility for converting webhook string to enum
inline LiveKitEventType parseLiveKitEvent(const std::string& event) {
    if (event == "room_started")        return LiveKitEventType::ROOM_STARTED;
    if (event == "room_finished")       return LiveKitEventType::ROOM_FINISHED;
    if (event == "participant_joined")  return LiveKitEventType::PARTICIPANT_JOINED;
    if (event == "participant_left")    return LiveKitEventType::PARTICIPANT_LEFT;

    return LiveKitEventType::UNKNOWN;
}

} // namespace livekit::webhook

#endif // LIVEKIT_WEBHOOK_LIVEKITEVENT_H
