#include "ChatServer.h"

#include <algorithm>
#include <ctime>
#include <random>

bool ChatServerState::createChannel(const std::string& name, const std::string& owner_id) {
    if (channels.find(name) != channels.end()) return false;  // already exists
    Channel ch;
    ch.name = name;
    ch.owner_id = owner_id;
    ch.is_persistent = true;
    channels[name] = ch;
    return true;
}

bool ChatServerState::destroyChannel(const std::string& name, const std::string& requester_id) {
    auto it = channels.find(name);
    if (it == channels.end()) return false;
    if (it->second.owner_id != requester_id) return false;  // only owner can destroy
    channels.erase(it);
    return true;
}

bool ChatServerState::joinChannel(const std::string& user_id, const std::string& channel_name) {
    auto ch_it = channels.find(channel_name);
    auto user_it = users.find(user_id);
    if (ch_it == channels.end() || user_it == users.end()) return false;
    // Remove user from previous channel
    if (!user_it->second.current_channel.empty()) {
        auto& old_ch = channels[user_it->second.current_channel];
        old_ch.user_ids.erase(user_id);
    }
    ch_it->second.user_ids.insert(user_id);
    user_it->second.current_channel = channel_name;
    return true;
}

bool ChatServerState::leaveChannel(const std::string& user_id) {
    auto user_it = users.find(user_id);
    if (user_it == users.end()) return false;
    std::string channel = user_it->second.current_channel;
    if (channel.empty()) return false;
    auto ch_it = channels.find(channel);
    if (ch_it == channels.end()) return false;
    ch_it->second.user_ids.erase(user_id);
    user_it->second.current_channel.clear();
    return true;
}

bool ChatServerState::sendMessage(const std::string& user_id, const std::string& text) {
    auto user_it = users.find(user_id);
    if (user_it == users.end()) return false;
    std::string channel = user_it->second.current_channel;
    if (channel.empty()) return false;
    auto ch_it = channels.find(channel);
    if (ch_it == channels.end()) return false;
    Message msg;
    msg.sender = user_it->second.username;
    msg.text = text;
    msg.timestamp = std::time(nullptr);
    ch_it->second.history.push_back(msg);
    return true;
}

std::vector<std::string> ChatServerState::listChannels() const {
    std::vector<std::string> names;
    for (const auto& kv : channels) {
        names.push_back(kv.first);
    }
    return names;
}

std::vector<Message> ChatServerState::getChannelHistory(const std::string& channel_name) const {
    auto it = channels.find(channel_name);
    if (it == channels.end()) return {};
    return it->second.history;
}

// Future WebRTC and call methods (stubs)
std::string ChatServerState::createCall(const std::string& initiator_id,
                                        const std::string& target_id, CallType type) {
    // TODO: Implement call creation
    // Generate unique call ID
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(1000, 9999);

    std::string call_id = "call_" + std::to_string(dis(gen));

    Call call(call_id, initiator_id, target_id, type);
    active_calls[call_id] = call;
    user_to_call[initiator_id] = call_id;
    user_to_call[target_id] = call_id;

    // Update user states
    auto initiator_it = users.find(initiator_id);
    auto target_it = users.find(target_id);
    if (initiator_it != users.end()) {
        initiator_it->second.active_call_id = call_id;
        initiator_it->second.state =
            (type == CallType::VOICE) ? ConnectionState::VOICE_CALL : ConnectionState::VIDEO_CALL;
    }
    if (target_it != users.end()) {
        target_it->second.active_call_id = call_id;
        target_it->second.state =
            (type == CallType::VOICE) ? ConnectionState::VOICE_CALL : ConnectionState::VIDEO_CALL;
    }

    return call_id;
}

bool ChatServerState::acceptCall(const std::string& call_id, const std::string& user_id) {
    // TODO: Implement call acceptance
    auto call_it = active_calls.find(call_id);
    if (call_it == active_calls.end()) return false;

    call_it->second.start_call();
    return true;
}

bool ChatServerState::rejectCall(const std::string& call_id, const std::string& user_id) {
    // TODO: Implement call rejection
    auto call_it = active_calls.find(call_id);
    if (call_it == active_calls.end()) return false;

    call_it->second.reject_call();
    return true;
}

bool ChatServerState::endCall(const std::string& call_id, const std::string& user_id) {
    // TODO: Implement call ending
    auto call_it = active_calls.find(call_id);
    if (call_it == active_calls.end()) return false;

    call_it->second.end_call();

    // Clean up user states
    for (const auto& participant_id : call_it->second.participant_ids) {
        auto user_it = users.find(participant_id);
        if (user_it != users.end()) {
            user_it->second.active_call_id.clear();
            user_it->second.state = ConnectionState::CHAT_ONLY;
            user_it->second.is_screen_sharing = false;
        }
        user_to_call.erase(participant_id);
    }

    active_calls.erase(call_it);
    return true;
}

bool ChatServerState::addWebRTCSignal(const std::string& call_id, const std::string& user_id,
                                      const std::string& sdp_data, MessageType signal_type) {
    // TODO: Implement WebRTC signaling
    auto call_it = active_calls.find(call_id);
    if (call_it == active_calls.end()) return false;

    if (signal_type == MessageType::WEBRTC_OFFER) {
        call_it->second.initiator_sdp = sdp_data;
    } else if (signal_type == MessageType::WEBRTC_ANSWER) {
        call_it->second.target_sdp = sdp_data;
    }

    return true;
}

bool ChatServerState::addIceCandidate(const std::string& call_id, const std::string& user_id,
                                      const std::string& candidate) {
    // TODO: Implement ICE candidate handling
    auto call_it = active_calls.find(call_id);
    if (call_it == active_calls.end()) return false;

    call_it->second.ice_candidates.push_back(candidate);
    return true;
}

bool ChatServerState::startScreenShare(const std::string& call_id, const std::string& user_id) {
    // TODO: Implement screen sharing start
    auto call_it = active_calls.find(call_id);
    if (call_it == active_calls.end()) return false;

    call_it->second.screen_sharing_active = true;
    call_it->second.screen_sharer_id = user_id;

    auto user_it = users.find(user_id);
    if (user_it != users.end()) {
        user_it->second.is_screen_sharing = true;
        user_it->second.state = ConnectionState::SCREEN_SHARING;
    }

    return true;
}

bool ChatServerState::stopScreenShare(const std::string& call_id, const std::string& user_id) {
    // TODO: Implement screen sharing stop
    auto call_it = active_calls.find(call_id);
    if (call_it == active_calls.end()) return false;

    call_it->second.screen_sharing_active = false;
    call_it->second.screen_sharer_id.clear();

    auto user_it = users.find(user_id);
    if (user_it != users.end()) {
        user_it->second.is_screen_sharing = false;
        // Restore previous state (voice or video call)
        if (user_it->second.active_call_id == call_id) {
            auto& call = call_it->second;
            user_it->second.state = (call.type == CallType::VOICE) ? ConnectionState::VOICE_CALL
                                                                   : ConnectionState::VIDEO_CALL;
        }
    }

    return true;
}

// Helper methods
Call* ChatServerState::getCall(const std::string& call_id) {
    auto it = active_calls.find(call_id);
    return (it != active_calls.end()) ? &it->second : nullptr;
}

Call* ChatServerState::getUserActiveCall(const std::string& user_id) {
    auto it = user_to_call.find(user_id);
    if (it == user_to_call.end()) return nullptr;
    return getCall(it->second);
}

bool ChatServerState::isUserInCall(const std::string& user_id) {
    return user_to_call.find(user_id) != user_to_call.end();
}

std::vector<std::string> ChatServerState::getCallParticipants(const std::string& call_id) {
    auto call_it = active_calls.find(call_id);
    if (call_it == active_calls.end()) return {};

    std::vector<std::string> participants;
    for (const auto& participant_id : call_it->second.participant_ids) {
        participants.push_back(participant_id);
    }
    return participants;
}