#pragma once
#include "common/Channel.h"
#include "common/Message.h"
#include "common/User.h"
#include "common/Hub.h"

#include <string>
#include <unordered_map>
#include <vector>

class ChatServerState {
   public:
    std::unordered_map<std::string, Hub> hubs;          // hub name -> Hub
    std::unordered_map<std::string, Channel> channels;  // channel name -> Channel
    std::unordered_map<std::string, User> users;        // user id -> User

    // Core methods (stubs)
    bool createHub(const std::string& name, const std::string& owner_id);
    bool destroyHub(const std::string& name, const std::string& requester_id);
    bool createChannel(const std::string& name, const std::string& owner_id);
    bool destroyChannel(const std::string& name, const std::string& requester_id);
    bool joinChannel(const std::string& user_id, const std::string& channel_name);
    bool leaveChannel(const std::string& user_id);
    bool sendMessage(const std::string& user_id, const std::string& text);
    std::vector<std::string> listChannels() const;
    std::vector<Message> getChannelHistory(const std::string& channel_name) const;
};