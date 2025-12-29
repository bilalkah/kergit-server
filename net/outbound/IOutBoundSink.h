// net/outbound/IOutboundSink.h
#ifndef NET_OUTBOUND_IOUTBOUNDSINK_H
#define NET_OUTBOUND_IOUTBOUNDSINK_H

#include "domains/ids/Ids.h"
#include "net/outbound/Msg.h"

#include <string>
#include <vector>

namespace net::outbound {

class IOutboundSink {
   public:
    virtual ~IOutboundSink() = default;

    virtual void send(const OutgoingMessage& msg) = 0;
};

}  // namespace net::outbound

#endif
