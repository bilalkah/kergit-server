#ifndef NET_TRANSPORT_WEBSOCKET_UTILS_H
#define NET_TRANSPORT_WEBSOCKET_UTILS_H

#include "proto/envelope.pb.h"
#include "proto/event/heartbeat.pb.h"

#include <optional>
#include <string>
#include <string_view>

namespace net::transport::websocket {

std::string_view app_pong_response_bytes();

}  // namespace net::transport::websocket
#endif  // NET_TRANSPORT_WEBSOCKET_UTILS_H
