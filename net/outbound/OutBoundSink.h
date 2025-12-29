// net/outbound/OutgoingQueueSink.h
#ifndef NET_OUTBOUND_OUTGOINGQUEUESINK_H
#define NET_OUTBOUND_OUTGOINGQUEUESINK_H

#include "net/outbound/IOutboundSink.h"
#include "net/outbound/OutgoingQueue.h"

namespace net::outbound {

class OutgoingQueueSink final : public IOutboundSink {
   public:
    explicit OutgoingQueueSink(OutgoingQueue& q) : queue_(q) {}

    void send(const OutgoingMessage& msg) override { queue_.push(msg); }

   private:
    OutgoingQueue& queue_;
};

}  // namespace net::outbound

#endif
