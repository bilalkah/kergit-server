#pragma once
#include "Message.h"

#include <string>
#include <unordered_set>
#include <vector>

enum class ChannelType { CHAT, VOICE };

class Channel {
   public:
    std::string name;
    std::string channel_id;
    std::string hub_id;
    ChannelType type;
    std::unordered_set<std::string> user_ids;  // or User*
};

class ChatChannel : public Channel {
   public:
    ChatChannel(std::string channel_name, std::string hub) {
        type = ChannelType::CHAT;
        name = channel_name;
        hub_id = hub;
    }

    void connect_user(const std::string& user_id) {
        if (user_ids.find(user_id) != user_ids.end()) {
            return;  // already connected
        }
        user_ids.insert(user_id);
    }
    void disconnect_user(const std::string& user_id) { user_ids.erase(user_id); }

    std::vector<Message> history;
    bool is_persistent = true;
};

class VoiceChannel : public Channel {
   public:
    VoiceChannel(std::string channel_name, std::string hub) {
        type = ChannelType::VOICE;
        name = channel_name;
        hub_id = hub;
    }

    void connect_user(const std::string& user_id) {
        if (user_ids.find(user_id) != user_ids.end()) {
            return;  // already connected
        }
        user_ids.insert(user_id);
    }
    void disconnect_user(const std::string& user_id) { user_ids.erase(user_id); }
};
