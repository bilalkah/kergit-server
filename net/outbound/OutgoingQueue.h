#ifndef NET_OUTBOUND_OUTGOINGQUEUE_H
#define NET_OUTBOUND_OUTGOINGQUEUE_H

#include "core/base/ThreadSafeQueue.h"
#include "net/outbound/Msg.h"

#include <string>
#include <variant>

namespace net::outbound {

using OutgoingQueue = ThreadSafeQueue<OutgoingMessage>;

}  // namespace net::outbound
#endif  // NET_OUTBOUND_OUTGOINGQUEUE_H
