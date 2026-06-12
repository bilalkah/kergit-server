#ifndef NET_TRANSPORT_HANDLES_H
#define NET_TRANSPORT_HANDLES_H

#include "net/transport/websocket/UwsTypes.h"

#include <string>
#include <string_view>

/**
 * WebSocket connection handles for connection
 *
 * These handles provide a uniform interface for sending messages,
 * closing connections, and pinging, abstracting away the underlying
 * WebSocket implementation details.
 *
 *
 * The handles internally use uWebSockets WebSocket pointers and call
 * the appropriate methods for sending data, closing connections, and
 * pinging. IT DEPENDS ON BE CALLED FROM THE LOOP THREAD. IF CALLED
 * FROM ANOTHER THREAD, USE ILoop::defer TO SCHEDULE THE CALL.
 *
 */
namespace net::transport {

struct WsHandle {
    using UwsSocket = websocket::UwsSocket;
    UwsSocket* ws{nullptr};

    bool valid() const { return ws != nullptr; }

    UwsSocket::SendStatus send(std::string payload, bool binary = false) const {
        if (ws) {
            return ws->send(payload, binary ? uWS::OpCode::BINARY : uWS::OpCode::TEXT);
        }
        return UwsSocket::SendStatus::DROPPED;
    }

    UwsSocket::SendStatus send(std::string_view payload, bool binary = false) const {
        if (ws) {
            return ws->send(payload, binary ? uWS::OpCode::BINARY : uWS::OpCode::TEXT);
        }
        return UwsSocket::SendStatus::DROPPED;
    }

    void end(const int code, const std::string reason) {
        if (ws) {
            ws->end(code, reason);
        }
    }

    void ping() {
        if (ws) {
            ws->send("", uWS::OpCode::PING);
        }
    }
};
}  // namespace net::transport

#endif  // NET_TRANSPORT_HANDLES_H
