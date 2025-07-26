#pragma once
#include "Call.h"
#include "Channel.h"
#include "Message.h"
#include "User.h"

#include <string>
#include <unordered_map>
#include <vector>

class ChatServerState {
   public:
    std::unordered_map<std::string, Channel> channels;  // channel name -> Channel
    std::unordered_map<std::string, User> users;        // user id -> User

    // Future WebRTC and call management
    std::unordered_map<std::string, Call> active_calls;         // call_id -> Call
    std::unordered_map<std::string, std::string> user_to_call;  // user_id -> call_id

    // Core methods (stubs)
    bool createChannel(const std::string& name, const std::string& owner_id);
    bool destroyChannel(const std::string& name, const std::string& requester_id);
    bool joinChannel(const std::string& user_id, const std::string& channel_name);
    bool leaveChannel(const std::string& user_id);
    bool sendMessage(const std::string& user_id, const std::string& text);
    std::vector<std::string> listChannels() const;
    std::vector<Message> getChannelHistory(const std::string& channel_name) const;

    // Future WebRTC and call methods (stubs)
    std::string createCall(const std::string& initiator_id, const std::string& target_id,
                           CallType type);
    bool acceptCall(const std::string& call_id, const std::string& user_id);
    bool rejectCall(const std::string& call_id, const std::string& user_id);
    bool endCall(const std::string& call_id, const std::string& user_id);
    bool addWebRTCSignal(const std::string& call_id, const std::string& user_id,
                         const std::string& sdp_data, MessageType signal_type);
    bool addIceCandidate(const std::string& call_id, const std::string& user_id,
                         const std::string& candidate);
    bool startScreenShare(const std::string& call_id, const std::string& user_id);
    bool stopScreenShare(const std::string& call_id, const std::string& user_id);

    // Helper methods
    Call* getCall(const std::string& call_id);
    Call* getUserActiveCall(const std::string& user_id);
    bool isUserInCall(const std::string& user_id);
    std::vector<std::string> getCallParticipants(const std::string& call_id);

    // ... more methods as needed
};