#ifndef NET_PERSOCKETDATA_H
#define NET_PERSOCKETDATA_H

#include "domains/ids/Ids.h"

#include <chrono>
#include <string>

namespace net {

struct PerSocketData {
    ConnId conn_id{""};
    UserId user_id{""};
    HubId current_hub_id{""};
    ChannelId current_channel_id{""};
    std::chrono::system_clock::time_point connected_at = std::chrono::system_clock::now();
    bool authenticated = false;
};

}  // namespace net

#endif  // NET_PERSOCKETDATA_Hp