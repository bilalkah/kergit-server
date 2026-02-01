#ifndef NET_CONNECTION_OUTBOUND_MSG_H
#define NET_CONNECTION_OUTBOUND_MSG_H

#include "domains/ids/Ids.h"

#include <chrono>
#include <string>
#include <variant>
#include <vector>

namespace net::outbound {

enum class OutboundPriority : uint8_t {
    High = 0,
    Low = 1,
};

struct Payload {
    std::string data;      // serialized bytes
    bool is_binary{true};  // default for protobuf
};

struct Target {
    std::vector<GlobalConnId> conns;

    static Target one(GlobalConnId c) { return {{std::move(c)}}; }

    static Target many(std::vector<GlobalConnId> cs) { return {std::move(cs)}; }
};

struct SendPayload {
    Payload payload;  // serialized, wire-ready
};

struct UpdateAuthState {
    bool is_authenticated{false};
    std::chrono::system_clock::time_point expires_at{};
};

struct DropConnection {
    int code = 0;
    std::string reason;

    DropConnection() = default;
    DropConnection(int code_in, std::string reason_in = {})
        : code(code_in), reason(std::move(reason_in)) {}
    DropConnection(const DropConnection&) = default;
    DropConnection& operator=(const DropConnection&) = default;
    DropConnection(DropConnection&& other) noexcept
        : code(other.code), reason(std::move(other.reason)) {}
    DropConnection& operator=(DropConnection&& other) noexcept {
        if (this != &other) {
            code = other.code;
            reason = std::move(other.reason);
        }
        return *this;
    }
};

using Action = std::variant<SendPayload, UpdateAuthState, DropConnection>;

struct OutgoingMessage {
    OutboundPriority priority{OutboundPriority::High};
    Target target;
    Action action;
};

}  // namespace net::outbound
#endif  // NET_CONNECTION_OUTBOUND_MSG_H
