#ifndef NET_PERSOCKETDATA_H
#define NET_PERSOCKETDATA_H

#include "domains/ids/Ids.h"

#include <cstdint>
#include <string>

namespace net::transport::websocket {

struct PerSocketData {
    ConnId conn_id{""};
    UserId user_id{""};
    std::string role{};
    int64_t exp{0};
};

}  // namespace net::transport::websocket

#endif  // NET_PERSOCKETDATA_H
