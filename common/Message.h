#pragma once
#include <ctime>
#include <string>

enum class MessageType {
    CHAT,
    CALL_REQUEST,
    CALL_ACCEPT,
    CALL_REJECT,
    CALL_END,
    WEBRTC_OFFER,
    WEBRTC_ANSWER,
    ICE_CANDIDATE,
    SCREEN_SHARE_START,
    SCREEN_SHARE_STOP,
    USER_STATE_CHANGE
};

class Message {
   public:
    std::string sender;
    std::string text;
    std::time_t timestamp;

    // Future WebRTC fields
    MessageType type = MessageType::CHAT;
    std::string call_id;
    std::string target_user;
    std::string media_type;  // "voice", "video", "screen"
    std::string sdp_data;    // WebRTC SDP
    std::string ice_candidate;

    // Helper methods for future use
    bool is_call_message() const {
        return type == MessageType::CALL_REQUEST || type == MessageType::CALL_ACCEPT ||
               type == MessageType::CALL_REJECT || type == MessageType::CALL_END;
    }

    bool is_webrtc_message() const {
        return type == MessageType::WEBRTC_OFFER || type == MessageType::WEBRTC_ANSWER ||
               type == MessageType::ICE_CANDIDATE;
    }
};