#ifndef NET_TRANSPORT_WEBSOCKET_UWSTYPES_H
#define NET_TRANSPORT_WEBSOCKET_UWSTYPES_H

#include "App.h"
#include "net/transport/websocket/TextPerSocketData.h"

namespace net::transport::websocket {

// Build-time switch for SSL
#ifdef USE_SSL
static constexpr bool kSslEnabled = true;
#else
static constexpr bool kSslEnabled = false;
#endif

// OpCode alias so upper layers don't include uWS directly
using UwsApp = std::conditional_t<kSslEnabled, uWS::SSLApp, uWS::App>;
using ListenToken = ::us_listen_socket_t*;

template <typename T>
using UwsWebSocketT = uWS::WebSocket<kSslEnabled, true, T>;
using UwsSocket = UwsWebSocketT<TextPerSocketData>;

}  // namespace net::transport::websocket
#endif  // NET_TRANSPORT_WEBSOCKET_UWSTYPES_H
