#ifndef NET_CONNECTION_OUTBOUND_MSG_H
#define NET_CONNECTION_OUTBOUND_MSG_H

#include "domains/ids/Ids.h"

#include <string>
#include <variant>

namespace net::outbound {

struct Payload {
    std::string data;
};

struct Target {
    std::vector<GlobalConnId> conns;

    static Target one(GlobalConnId c) { return {{std::move(c)}}; }

    static Target many(std::vector<GlobalConnId> cs) { return {std::move(cs)}; }
};

struct SendPayload {
    Payload payload;
};

struct DropConnection {
    int code;
    std::string reason;
};

using Action = std::variant<SendPayload, DropConnection>;

struct OutgoingMessage {
    Target target;
    Action action;
};

}  // namespace net::outbound
#endif  // NET_CONNECTION_OUTBOUND_MSG_H
