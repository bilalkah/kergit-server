
#ifndef NET_TRANSPORT_ILOOP_H
#define NET_TRANSPORT_ILOOP_H

#include "App.h"

#include <functional>
#include <string>

namespace net::transport {

class ILoop {
   public:
    virtual ~ILoop() = default;

    // Defer a task onto the loop (safe cross-thread send, etc.)
    using DeferFn = std::function<void()>;
    virtual void defer(DeferFn fn) = 0;

    // Get the underlying uWS Loop pointer
    virtual uWS::Loop* getUwsLoop() = 0;
};

}  // namespace net::transport

#endif  // NET_TRANSPORT_ILOOP_H
