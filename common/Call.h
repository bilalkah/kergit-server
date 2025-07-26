#pragma once
#include <ctime>
#include <string>
#include <unordered_set>
#include <vector>

enum class CallType { VOICE, VIDEO, SCREEN_SHARE };

enum class CallState { REQUESTING, RINGING, ACTIVE, ENDED, REJECTED };

class Call {
   public:
    std::string id;
    std::string initiator_id;
    std::string target_id;
    CallType type;
    CallState state = CallState::REQUESTING;
    std::time_t created_at;
    std::time_t started_at;
    std::time_t ended_at;

    // Participants in the call
    std::unordered_set<std::string> participant_ids;

    // WebRTC session data
    std::string initiator_sdp;
    std::string target_sdp;
    std::vector<std::string> ice_candidates;

    // Screen sharing state
    bool screen_sharing_active = false;
    std::string screen_sharer_id;

    // Default constructor for unordered_map compatibility
    Call() : type(CallType::VOICE), created_at(0), started_at(0), ended_at(0) {}

    Call(const std::string& call_id, const std::string& initiator, const std::string& target,
         CallType call_type)
        : id(call_id),
          initiator_id(initiator),
          target_id(target),
          type(call_type),
          created_at(std::time(nullptr)) {
        participant_ids.insert(initiator);
        participant_ids.insert(target);
    }

    // Helper methods
    bool is_participant(const std::string& user_id) const {
        return participant_ids.find(user_id) != participant_ids.end();
    }

    bool is_active() const { return state == CallState::ACTIVE; }

    void start_call() {
        state = CallState::ACTIVE;
        started_at = std::time(nullptr);
    }

    void end_call() {
        state = CallState::ENDED;
        ended_at = std::time(nullptr);
    }

    void reject_call() {
        state = CallState::REJECTED;
        ended_at = std::time(nullptr);
    }
};