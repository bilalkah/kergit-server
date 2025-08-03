#pragma once
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

    // Core methods (stubs)
    bool createChannel(const std::string& name, const std::string& owner_id);
    bool destroyChannel(const std::string& name, const std::string& requester_id);
    bool joinChannel(const std::string& user_id, const std::string& channel_name);
    bool leaveChannel(const std::string& user_id);
    bool sendMessage(const std::string& user_id, const std::string& text);
    std::vector<std::string> listChannels() const;
    std::vector<Message> getChannelHistory(const std::string& channel_name) const;
};