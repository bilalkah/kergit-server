#ifndef NET_PERSOCKETDATA_H
#define NET_PERSOCKETDATA_H

#include "domains/Hub.h"
#include "domains/ids/Ids.h"

#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace net {

struct Snapshot {
    UserId user_id{""};
    std::string email{""};
    std::string username{""};

    std::unordered_set<HubId> hubs{};
    std::unordered_map<HubId, Role> roles{};
    ChannelId current_voice_channel_id{""};
    ChannelId current_text_channel_id{""};
    HubId current_hub_id{""};

    bool authenticated = false;
    std::chrono::system_clock::time_point authenticated_at{};
};

struct PerSocketData {
    ConnId conn_id{""};

    // Application-level fields
    std::shared_ptr<const Snapshot> snapshot = std::make_shared<Snapshot>();

    // flag fields
    bool alive = false;

    // Timestamps
    std::chrono::system_clock::time_point connected_at = std::chrono::system_clock::now();
    std::chrono::system_clock::time_point last_ping_at = std::chrono::system_clock::now();
    std::chrono::system_clock::time_point last_pong_at = std::chrono::system_clock::now();
    std::chrono::milliseconds rtt_ms{0};
};

}  // namespace net

#endif  // NET_PERSOCKETDATA_H
