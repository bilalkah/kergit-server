#ifndef NET_TRANSPORT_ITRANSPORTSERVER_H
#define NET_TRANSPORT_ITRANSPORTSERVER_H

namespace net::transport {

class ITransportServer {
public:
    virtual ~ITransportServer() = default;

    // lifecycle
    virtual void start() = 0;
    virtual void stop() = 0;

    // identity
    virtual const char* name() const = 0;

    // optional: expose loop/thread identity if needed
    virtual void* loop_id() const = 0;
};

} // namespace net::transport

#endif // NET_TRANSPORT_ITRANSPORTSERVER_H
