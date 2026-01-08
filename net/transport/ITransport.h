#ifndef NET_TRANSPORT_ITRANSPORTSERVER_H
#define NET_TRANSPORT_ITRANSPORTSERVER_H

#include "domains/ids/Ids.h"

#include <cstdint>
#include <functional>
#include <string_view>

namespace net::transport {

struct Hooks {
    std::function<void(const ConnId& conn_id, const UserId& user_id)> on_open;
    std::function<void(const ConnId& conn_id, std::string_view raw)> on_message;
    std::function<void(const ConnId& conn_id, int code, std::string_view reason)> on_close;
};

class ITransportServer {
   public:
    virtual ~ITransportServer() = default;

    // lifecycle
    virtual void start() = 0;
    virtual void stop() = 0;

    // access lifecycle
    virtual bool is_started() const = 0;
    virtual bool is_stopped() const = 0;

    // identity
    virtual const char* name() const = 0;

    // optional: expose loop/thread identity if needed
    virtual void* loop_id() const = 0;

    // hooks
    virtual void set_hooks(Hooks hooks) = 0;
};

}  // namespace net::transport

#endif  // NET_TRANSPORT_ITRANSPORTSERVER_H
