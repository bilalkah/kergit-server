#ifndef NET_PERSOCKETDATA_H
#define NET_PERSOCKETDATA_H

#include "domains/Hub.h"
#include "domains/ids/Ids.h"

#include <chrono>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>

namespace net {

struct Snapshot {
    std::unordered_set<HubId> hubs;
    std::unordered_map<HubId, Role> roles;
    std::unordered_set<ChannelId> channels;
};

struct PerSocketData {
    ConnId conn_id{""};
    UserId user_id{""};
    HubId current_hub_id{""};
    ChannelId current_channel_id{""};
    std::string email{};
    std::string username{};

    // Subscriptions
    std::shared_ptr<const Snapshot> snapshot;
    
    // flag fields
    bool authenticated = false;
    bool alive = false;

    // Timestamps
    std::chrono::system_clock::time_point connected_at = std::chrono::system_clock::now();
    std::chrono::system_clock::time_point last_ping_at = std::chrono::system_clock::now();
    std::chrono::system_clock::time_point last_pong_at = std::chrono::system_clock::now();
    std::chrono::system_clock::time_point authenticated_at{};
    std::chrono::milliseconds rtt_ms{0};
};

}  // namespace net

#endif  // NET_PERSOCKETDATA_H
