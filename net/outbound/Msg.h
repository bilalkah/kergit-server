#ifndef NET_CONNECTION_OUTBOUND_MSG_H
#define NET_CONNECTION_OUTBOUND_MSG_H

#include "domains/ids/Ids.h"

#include <chrono>
#include <string>
#include <variant>
#include <vector>

namespace net::outbound {

struct Payload {
    std::string data;
    bool is_binary{false};
};

struct Target {
    std::vector<GlobalConnId> conns;

    static Target one(GlobalConnId c) { return {{std::move(c)}}; }

    static Target many(std::vector<GlobalConnId> cs) { return {std::move(cs)}; }
};

struct SendPayload {
    Payload payload;
};

struct UpdateAuthState {
    bool is_authenticated{false};
    std::chrono::system_clock::time_point expires_at{};
};

struct DropConnection {
    int code;
    std::string reason;
};

using Action = std::variant<SendPayload, UpdateAuthState, DropConnection>;

struct OutgoingMessage {
    Target target;
    Action action;
};

}  // namespace net::outbound
#endif  // NET_CONNECTION_OUTBOUND_MSG_H
