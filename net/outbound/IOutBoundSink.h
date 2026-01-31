// net/outbound/IOutboundSink.h
#ifndef NET_OUTBOUND_IOUTBOUNDSINK_H
#define NET_OUTBOUND_IOUTBOUNDSINK_H

#include "net/outbound/Msg.h"

namespace net::outbound {

enum class PushResult {
    Ok,
    DroppedLowPriority,
    DroppedHighPriority,
};

class IOutboundSink {
   public:
    virtual ~IOutboundSink() = default;

    virtual PushResult push(const OutgoingMessage& msg) = 0;

    virtual PushResult push(OutgoingMessage&& msg) = 0;
};

}  // namespace net::outbound

#endif
