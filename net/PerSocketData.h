#ifndef NET_PERSOCKETDATA_H
#define NET_PERSOCKETDATA_H

#include "domains/ids/Ids.h"

#include <chrono>
#include <string>
#include <unordered_set>

namespace net {

struct PerSocketData {
    ConnId conn_id{""};
    UserId user_id{""};
    HubId current_hub_id{""};
    ChannelId current_channel_id{""};
    std::chrono::system_clock::time_point connected_at = std::chrono::system_clock::now();
    std::chrono::steady_clock::time_point last_pong_at = std::chrono::steady_clock::now();
    std::chrono::system_clock::time_point authenticated_at{};
    std::string email{};
    std::string username{};
    std::unordered_set<HubId> hub_memberships;
    bool authenticated = false;
};

}  // namespace net

#endif  // NET_PERSOCKETDATA_H
