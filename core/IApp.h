
#ifndef CORE_IAPP_H
#define CORE_IAPP_H

#include "core/Types.h"

#include <functional>
#include <string>

namespace core {

class IApp {
   public:
    virtual ~IApp() = default;

    // Access the underlying uWS app to register routes
    virtual UwsApp& uws() = 0;

    // Defer a task onto the app's loop (safe cross-thread send, etc.)
    using DeferFn = std::function<void()>;
    virtual void defer(DeferFn fn) = 0;
};

}  // namespace core

#endif  // CORE_IAPP_H
