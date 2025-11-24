#ifndef APP_QUEUE_OUTGOING_QUEUE_H
#define APP_QUEUE_OUTGOING_QUEUE_H

#include "app/queue/ThreadSafeCmdQueue.h"
#include "domains/ids/Ids.h"

#include <functional>
#include <string>
#include <variant>

namespace net {
struct PerSocketData;
}

struct DirectMessage {
    ConnId conn_id;
    std::string payload;

    std::function<void(net::PerSocketData*)> apply_psd;
};

struct PublishMessage {
    std::string topic;
    std::string payload;
};

// Outgoing messages are one of these two
using OutgoingMessage = std::variant<DirectMessage, PublishMessage>;

using OutgoingQueue = ThreadSafeQueue<OutgoingMessage>;

#endif  // APP_QUEUE_OUTGOING_QUEUE_H
