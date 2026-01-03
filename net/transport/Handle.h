#ifndef NET_TRANSPORT_HANDLES_H
#define NET_TRANSPORT_HANDLES_H

#include "net/transport/websocket/UwsTypes.h"

#include <string>
#include <variant>

/**
 * WebSocket connection handles for different connection types
 * (e.g., text-based, voice-based, etc.)
 *
 * These handles provide a uniform interface for sending messages,
 * closing connections, and pinging, abstracting away the underlying
 * WebSocket implementation details.
 *
 * Later, we can extend this to support additional connection types
 * as needed.
 *
 * The handles internally use uWebSockets WebSocket pointers and call
 * the appropriate methods for sending data, closing connections, and
 * pinging. IT DEPENDS ON BE CALLED FROM THE LOOP THREAD. IF CALLED
 * FROM ANOTHER THREAD, USE ILoop::defer TO SCHEDULE THE CALL.
 *
 */
namespace net::transport {

struct TextWsHandle {
    using UwsSocket = websocket::UwsSocket;
    UwsSocket* ws{nullptr};

    bool valid() const { return ws != nullptr; }

    void send(std::string payload) {
        if (ws) {
            ws->send(payload, uWS::OpCode::TEXT);
        }
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

// Later: VoiceWsHandle for voice connections
// Dummy for now
struct VoiceWsHandle {
    using UwsSocket = websocket::UwsSocket;
    UwsSocket* ws{nullptr};

    bool valid() const { return ws != nullptr; }

    void send(std::string payload) {
        if (ws) {
            ws->send(payload, uWS::OpCode::BINARY);
        }
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

using ConnHandle = std::variant<TextWsHandle, VoiceWsHandle>;
}  // namespace net::transport

#endif  // NET_TRANSPORT_HANDLES_H