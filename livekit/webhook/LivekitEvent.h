#ifndef LIVEKIT_WEBHOOK_LIVEKITEVENT_H
#define LIVEKIT_WEBHOOK_LIVEKITEVENT_H

#include <cstdint>
#include <iostream>
#include <string>

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
    PARTICIPANT_CONNECTION_ABORTED,
};

// Normalized event forwarded to the application layer
struct LiveKitEvent {
    LiveKitEventType type = LiveKitEventType::UNKNOWN;

    // Unique webhook id from LiveKit payload.
    std::string event_id;

    // LiveKit room corresponds to our voice channel
    ChannelId channel_id;

    // User identity from LiveKit participant identity
    UserId user_id;

    // Participant SID from webhook payload.
    std::string participant_sid;

    // Raw participant metadata string.
    std::string participant_metadata;

    // Session correlation data parsed from participant metadata.
    uint64_t app_session_id = 0;
    std::string intent_nonce;

    // The LiveKit node that owns the room (e.g. "ND_xxxx")
    std::string node_id;

    uint64_t timestamp_ms = 0;

    friend std::ostream& operator<<(std::ostream& os, const LiveKitEvent& e) {
        os << "LiveKitEvent{type=" << static_cast<int>(e.type)
           << ", event_id=" << e.event_id << ", channel_id=" << e.channel_id.value
           << ", user_id=" << e.user_id.value << ", participant_sid=" << e.participant_sid
           << ", app_session_id=" << e.app_session_id << ", intent_nonce=" << e.intent_nonce
           << ", node_id=" << e.node_id << ", timestamp_ms=" << e.timestamp_ms << "}";
        return os;
    }
};

// Utility for converting webhook string to enum
inline LiveKitEventType parseLiveKitEvent(const std::string& event) {
    if (event == "room_started") return LiveKitEventType::ROOM_STARTED;
    if (event == "room_finished") return LiveKitEventType::ROOM_FINISHED;
    if (event == "participant_joined") return LiveKitEventType::PARTICIPANT_JOINED;
    if (event == "participant_left") return LiveKitEventType::PARTICIPANT_LEFT;
    if (event == "participant_connection_aborted")
        return LiveKitEventType::PARTICIPANT_CONNECTION_ABORTED;

    return LiveKitEventType::UNKNOWN;
}

}  // namespace livekit::webhook

#endif  // LIVEKIT_WEBHOOK_LIVEKITEVENT_H
