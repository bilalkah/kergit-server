#ifndef APP_QUEUE_EVENT_QUEUE_H
#define APP_QUEUE_EVENT_QUEUE_H

#include "app/queue/ThreadSafeCmdQueue.h"
#include "domains/ids/Ids.h"

#include <variant>

struct CommandRequest {
    ConnId conn_id;
    UserId user_id;
    HubId current_hub_id;
    ChannelId current_channel_id;
    bool authenticated{false};
    
    std::string payload;
    std::chrono::system_clock::time_point received_at;
};

namespace net {
struct Snapshot;
}  // namespace net

struct DisconnectEvent {
    ConnId conn_id;
    UserId user_id;
    std::shared_ptr<const net::Snapshot> snap;  // hubs/channels/roles
    
    std::string display;
    int code{};
    std::string reason;
};

using EventPayload = std::variant<CommandRequest, DisconnectEvent>;

struct Event {
    EventPayload payload;
};

using EventQueue = ThreadSafeQueue<Event>;

#endif  // APP_QUEUE_EVENT_QUEUE_H
