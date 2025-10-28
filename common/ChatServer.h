#pragma once
#include "domains/Channel.h"
#include "domains/Hub.h"
#include "domains/Message.h"
#include "domains/User.h"
#include "domains/ids/Ids.h"

#include <string>
#include <unordered_map>
#include <vector>

class ChatServerState {
   public:
    std::unordered_map<HubId, Hub> hubs;          // hub name -> Hub
    std::unordered_map<ChannelId, Channel> channels;  // channel name -> Channel
    std::unordered_map<UserId, User> users;        // user id -> User

    // Core methods (stubs)
    bool createChannel(const std::string& name, const HubId& hub_id);
    bool destroyChannel(const std::string& name, const UserId& requester_id);
    bool joinChannel(const UserId& user_id, const std::string& channel_name);
    bool leaveChannel(const UserId& user_id);
    bool sendMessage(const UserId& user_id, const std::string& text);
    std::vector<std::string> listChannels() const;
    std::vector<Message> getChannelHistory(const std::string& channel_name) const;
};