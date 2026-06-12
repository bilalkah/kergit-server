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

    TRACK_PUBLISHED,
    TRACK_UNPUBLISHED,

    EGRESS_STARTED,
    EGRESS_UPDATED,
    EGRESS_ENDED,

    INGRESS_STARTED,
    INGRESS_ENDED,
};

// Normalized event forwarded to the application layer
struct LiveKitEvent {
    LiveKitEventType type = LiveKitEventType::UNKNOWN;

    // Unique webhook id from LiveKit payload.
    std::string event_id;

    // Raw webhook event name from payload.event.
    std::string raw_event_name;

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

    // Assigned application LiveKit node from participant metadata.
    // This is minted by our token service as metadata.node_id; it is not LiveKit's
    // authoritative physical room owner.
    std::string node_id;

    // Optional track payload details for track_* events.
    std::string track_sid;
    std::string track_type;
    std::string track_source;

    // Optional egress/ingress identifiers for egress_* and ingress_* events.
    std::string egress_id;
    std::string ingress_id;

    uint64_t timestamp_ms = 0;

    friend std::ostream& operator<<(std::ostream& os, const LiveKitEvent& e) {
        os << "LiveKitEvent{type=" << static_cast<int>(e.type)
           << ", event_id=" << e.event_id << ", raw_event_name=" << e.raw_event_name
           << ", channel_id=" << e.channel_id.value
           << ", user_id=" << e.user_id.value << ", participant_sid=" << e.participant_sid
           << ", app_session_id=" << e.app_session_id << ", intent_nonce=" << e.intent_nonce
           << ", node_id=" << e.node_id << ", track_sid=" << e.track_sid
           << ", track_type=" << e.track_type << ", track_source=" << e.track_source
           << ", egress_id=" << e.egress_id << ", ingress_id=" << e.ingress_id
           << ", timestamp_ms=" << e.timestamp_ms << "}";
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
    if (event == "track_published") return LiveKitEventType::TRACK_PUBLISHED;
    if (event == "track_unpublished") return LiveKitEventType::TRACK_UNPUBLISHED;
    if (event == "egress_started") return LiveKitEventType::EGRESS_STARTED;
    if (event == "egress_updated") return LiveKitEventType::EGRESS_UPDATED;
    if (event == "egress_ended") return LiveKitEventType::EGRESS_ENDED;
    if (event == "ingress_started") return LiveKitEventType::INGRESS_STARTED;
    if (event == "ingress_ended") return LiveKitEventType::INGRESS_ENDED;

    return LiveKitEventType::UNKNOWN;
}

}  // namespace livekit::webhook

#endif  // LIVEKIT_WEBHOOK_LIVEKITEVENT_H
