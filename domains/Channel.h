#ifndef DOMAINS_CHANNEL_H
#define DOMAINS_CHANNEL_H

#pragma once
#include "domains/ids/Ids.h"

#include <string>
#include <unordered_set>

enum class ChannelType { CHAT, VOICE };

class Channel {
   public:
    std::string name{};
    ChannelId channel_id{""};             // in-class default
    HubId hub_id{""};                     // in-class default
    ChannelType type{ChannelType::CHAT};  // in-class default
    std::unordered_set<UserId> member_user_ids{};

    Channel() = default;  // now trivial and fine

    Channel(std::string channel_name, ChannelId channel_id, HubId hub_id, ChannelType channel_type)
        : name(std::move(channel_name)),
          channel_id(std::move(channel_id)),
          hub_id(std::move(hub_id)),
          type(channel_type) {}

    Channel(const Channel&) = default;

    bool hasMember(const UserId& uid) const { return member_user_ids.find(uid) != member_user_ids.end(); }
    bool addMember(const UserId& uid) { return member_user_ids.insert(uid).second; }
    bool removeMember(const UserId& uid) { return member_user_ids.erase(uid) > 0; }

    bool is_initialized() const noexcept {
        return !channel_id.value.empty() && !hub_id.value.empty();
    }
};

#endif  // DOMAINS_CHANNEL_H
