#ifndef NET_TRANSPORT_WEBSOCKET_UTILS_H
#define NET_TRANSPORT_WEBSOCKET_UTILS_H

#include "proto/envelope.pb.h"
#include "proto/system/heartbeat.pb.h"

#include <expected>
#include <string>
#include <string_view>

namespace net::transport::websocket {

std::string trim_ws(std::string_view value);

std::string extract_token(std::string_view protocols);

std::expected<std::string, std::string> make_app_pong_response(
    const sercom::protocol::Envelope& env);

}  // namespace net::transport::websocket
#endif  // NET_TRANSPORT_WEBSOCKET_UTILS_H
