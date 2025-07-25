#pragma once
#include <unordered_map>
#include <string>
#include "User.h"
#include "Channel.h"

class ChatServer {
public:
    std::unordered_map<std::string, Channel> channels; // channel name -> Channel
    std::unordered_map<std::string, User> users;       // user id -> User

    // Core methods (stubs)
    bool createChannel(const std::string& name, const std::string& owner_id);
    bool destroyChannel(const std::string& name, const std::string& requester_id);
    bool joinChannel(const std::string& user_id, const std::string& channel_name);
    bool leaveChannel(const std::string& user_id);
    bool sendMessage(const std::string& user_id, const std::string& text);
    std::vector<std::string> listChannels() const;
    std::vector<Message> getChannelHistory(const std::string& channel_name) const;
    // ... more methods as needed
}; 