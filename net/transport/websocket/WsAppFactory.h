#ifndef NET_TRANSPORT_WEBSOCKET_IWSAPPFACTORY_H
#define NET_TRANSPORT_WEBSOCKET_IWSAPPFACTORY_H

#include "core/ServerConfig.h"
#include "net/transport/websocket/IWsApp.h"
#include "utils/Loggable.h"
#include "utils/TlsConfig.h"

#include <memory>

namespace net::transport::websocket {

class PlainApp : public IApp, utils::Loggable {
   public:
    PlainApp() : app_() {
        log(utils::LogLevel::WARN, "PlainApp created. SSL ", kSslEnabled ? "enabled" : "disabled");
    }
    UwsApp& uws() override { return app_; }
    void defer(DeferFn fn) override {
        if (auto* loop = app_.getLoop()) {
            // Ensure work is scheduled onto the app's loop (even when called cross-thread)
            loop->defer(std::move(fn));
        }
    }

    uWS::Loop* getUwsLoop() override { return app_.getLoop(); }

   private:
    UwsApp app_;
};

class TLSApp : public IApp, utils::Loggable {
   public:
    explicit TLSApp(const uWS::SocketContextOptions& opts) : app_(opts) {
        log(utils::LogLevel::WARN, "TLSApp created with SSL context. SSL ",
            kSslEnabled ? "enabled" : "disabled");
    }
    UwsApp& uws() override { return app_; }
    void defer(DeferFn fn) override {
        if (auto* loop = app_.getLoop()) {
            loop->defer(std::move(fn));
        }
    }

    uWS::Loop* getUwsLoop() override { return app_.getLoop(); }

   private:
    UwsApp app_;
};

struct AppFactory {
    static std::unique_ptr<IApp> create(const core::NetworkStackConfig& cfg) {
        if (cfg.tls.validate_and_log()) {
            uWS::SocketContextOptions opts{
                .key_file_name = cfg.tls.key_path.c_str(),
                .cert_file_name = cfg.tls.cert_path.c_str(),
            };
            return std::make_unique<TLSApp>(opts);
        } else {
            return std::make_unique<PlainApp>();
        }
    }
};

}  // namespace net::transport::websocket

#endif  // NET_TRANSPORT_WEBSOCKET_IWSAPPFACTORY_H
