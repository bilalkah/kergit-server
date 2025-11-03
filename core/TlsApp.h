#ifndef CORE_TLSAPP_H
#define CORE_TLSAPP_H

#include "core/IApp.h"
#include "utils/Loggable.h"
#include "utils/TlsConfig.h"

#include <memory>

namespace core {

class TLSApp : public IApp, utils::Loggable {
   public:
    explicit TLSApp(const uWS::SocketContextOptions& opts) : app_(opts) {
        log(utils::LogLevel::WARN, "TLSApp created with SSL context. SSL ",
            kSslEnabled ? "enabled" : "disabled");
    }
    UwsApp& uws() override { return app_; }
    void defer(DeferFn fn) override { uWS::Loop::get()->defer(std::move(fn)); }

   private:
    UwsApp app_;
};

}  // namespace core

#endif  // CORE_TLSAPP_H
