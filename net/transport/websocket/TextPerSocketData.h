#ifndef NET_PERSOCKETDATA_H
#define NET_PERSOCKETDATA_H

#include "domains/ids/Ids.h"

namespace net::transport::websocket {

struct TextPerSocketData {
    ConnId conn_id{""};
};

}  // namespace net::transport::websocket

#endif  // NET_PERSOCKETDATA_H
