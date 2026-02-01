#ifndef NET_TRANSPORT_IOUTBOUNDTRANSPORT_H
#define NET_TRANSPORT_IOUTBOUNDTRANSPORT_H

#include "net/transport/Handle.h"

#include <string_view>

namespace net::transport {

struct IOutboundTransport {
    virtual bool send(transport::WsHandle& handle, std::string_view payload,
                      bool binary) noexcept = 0;
    virtual bool is_backpressured(const transport::WsHandle& handle) const noexcept = 0;
    virtual ~IOutboundTransport() = default;
};

}  // namespace net::transport

#endif  // NET_TRANSPORT_IOUTBOUNDTRANSPORT_H
