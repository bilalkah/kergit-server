#ifndef APP_QUEUE_MSG_H
#define APP_QUEUE_MSG_H

#include "domains/ids/Ids.h"

#include <cstdint>
#include <string>
#include <variant>

namespace app::queue {

struct Payload {
    std::string data;
};

struct ConnectionEvent {
    UserId user_id;
};

struct DisconnectionEvent {
    int code{};
    std::string reason;
};

struct MessageEvent {
    Payload payload;
};

using EventBody = std::variant<ConnectionEvent, DisconnectionEvent, MessageEvent>;

struct Event {
    GlobalConnId conn_id;
    EventBody body;
};

}  // namespace app::queue

#endif  // APP_QUEUE_MSG_H
