#ifndef APP_QUEUE_EVENT_QUEUE_H
#define APP_QUEUE_EVENT_QUEUE_H

#include "app/queue/ThreadSafeCmdQueue.h"
#include "domains/ids/Ids.h"

#include <variant>

namespace net {
struct Snapshot;
}  // namespace net

struct CommandRequest {
    ConnId conn_id;

    std::shared_ptr<const net::Snapshot> snapshot;

    std::string payload;
    std::chrono::system_clock::time_point received_at;
};

namespace net {
struct Snapshot;
}  // namespace net

struct DisconnectEvent {
    ConnId conn_id;
    std::shared_ptr<const net::Snapshot> snap;

    int code{};
    std::string reason;
};

struct ConnectEvent {
    ConnId conn_id;
    std::shared_ptr<const net::Snapshot> snap;
};

using EventPayload = std::variant<CommandRequest, ConnectEvent, DisconnectEvent>;

struct Event {
    EventPayload payload;
};

using EventQueue = ThreadSafeQueue<Event>;

#endif  // APP_QUEUE_EVENT_QUEUE_H
