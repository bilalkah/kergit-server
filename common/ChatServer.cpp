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
    msg.timestamp = std::chrono::system_clock::now();
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
