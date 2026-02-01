#ifndef NET_OUTBOUND_OUTBOUNDFLUSHENGINE_H
#define NET_OUTBOUND_OUTBOUNDFLUSHENGINE_H

#include <chrono>

struct us_timer_t;

namespace net::connection {
class ConnectionRegistery;
}

namespace net::transport {
struct IOutboundTransport;
}

namespace net::outbound {

class OutboundFlushEngine {
   public:
    OutboundFlushEngine(connection::ConnectionRegistery& registry,
                        transport::IOutboundTransport& transport,
                        std::chrono::milliseconds tick);
    ~OutboundFlushEngine() = default;

    void start();
    void stop();

   private:
    static void on_timer(us_timer_t* timer);
    void on_tick();

    connection::ConnectionRegistery& registry_;
    transport::IOutboundTransport& transport_;
    std::chrono::milliseconds tick_;
    ::us_timer_t* timer_{nullptr};
};

}  // namespace net::outbound

#endif  // NET_OUTBOUND_OUTBOUNDFLUSHENGINE_H
