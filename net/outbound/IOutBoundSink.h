// net/outbound/IOutboundSink.h
#ifndef NET_OUTBOUND_IOUTBOUNDSINK_H
#define NET_OUTBOUND_IOUTBOUNDSINK_H

#include "net/outbound/Msg.h"

namespace net::outbound {

class IOutboundSink {
   public:
    virtual ~IOutboundSink() = default;

    virtual void push(const OutgoingMessage& msg) = 0;

    virtual void push(OutgoingMessage&& msg) = 0;
};

}  // namespace net::outbound

#endif
