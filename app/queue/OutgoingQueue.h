#ifndef APP_QUEUE_OUTGOING_QUEUE_H
#define APP_QUEUE_OUTGOING_QUEUE_H

#include "app/queue/ThreadSafeCmdQueue.h"
#include "domains/ids/Ids.h"

namespace net {
struct PerSocketData;
}

struct OutgoingMessage {
    ConnId conn_id;

    std::string payload;

    std::function<void(net::PerSocketData&)> apply_psd;
};

using OutgoingQueue = ThreadSafeQueue<OutgoingMessage>;

#endif  // APP_QUEUE_OUTGOING_QUEUE_H
