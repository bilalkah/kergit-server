#pragma once
#include <string>

enum class Authority { USER, MODERATOR, ADMIN };

enum class ConnectionState { CHAT_ONLY, VOICE_CALL, VIDEO_CALL, SCREEN_SHARING };

class User {
   public:
    std::string id;
    std::string username;
    Authority authority = Authority::USER;
    std::string current_channel;

    // Future WebRTC fields
    ConnectionState state = ConnectionState::CHAT_ONLY;
    std::string active_call_id;
    bool is_screen_sharing = false;
    std::string webrtc_session_id;

    // Add more fields as needed (e.g., connection handle)
};