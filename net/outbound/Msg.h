#ifndef NET_CONNECTION_OUTBOUND_MSG_H
#define NET_CONNECTION_OUTBOUND_MSG_H

#include "domains/ids/Ids.h"

#include <string>
#include <variant>

namespace net::outbound {

struct Payload {
    std::string data;
};

struct DirectMessage {
    ConnId conn_id;
    Payload payload;
};

struct PublishMessage {
    std::vector<ConnId> conn_ids;
    Payload payload;
};

// Outgoing messages are one of these two
using OutgoingMessage = std::variant<DirectMessage, PublishMessage>;

}  // namespace net::outbound
#endif  // NET_CONNECTION_OUTBOUND_MSG_H
