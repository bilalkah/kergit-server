#ifndef APP_QUEUE_MSG_H
#define APP_QUEUE_MSG_H

#include "domains/ids/Ids.h"
#include "proto/envelope.pb.h"

#include <cstdint>
#include <string>
#include <variant>

namespace app::queue {

struct Payload {
    sercom::protocol::Envelope env;
};

struct ConnectionEvent {
    GlobalConnId conn_id;
    UserId user_id;
};

struct DisconnectionEvent {
    GlobalConnId conn_id;
    int code{};
    std::string reason;
};

struct MessageEvent {
    GlobalConnId conn_id;
    Payload payload;
};

using Event = std::variant<ConnectionEvent, DisconnectionEvent, MessageEvent>;

}  // namespace app::queue

#endif  // APP_QUEUE_MSG_H
