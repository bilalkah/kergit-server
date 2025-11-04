#ifndef NET_WEBSOCKETSERVER_H
#define NET_WEBSOCKETSERVER_H

#include "app/Dispatcher.h"
#include "core/IApp.h"
#include "core/Types.h"
#include "infra/security/validation/MessageValidator.h"
#include "net/ClientGateway.h"
#include "net/ConnectionManager.h"
#include "net/Heartbeat.h"
#include "net/PerSocketData.h"

#include <functional>
#include <memory>
#include <string>

namespace app::services {
class HubPublisher;
}

namespace net {

struct WsLimits {
    size_t max_message_bytes = 256 * 1024;
};

struct OriginAllowlist {
    bool is_allowed(const std::string& origin) const {
        if (origin.empty()) return true;
        return origin.find("http://localhost") == 0 || origin.find("https://localhost") == 0;
    }
};

struct WsHooks {
    std::function<void(const ConnId& conn_id)> on_open;
    std::function<void(const ConnId& conn_id, int code, std::string_view reason)> on_close;
    std::function<void(const ConnId& conn_id, std::string_view raw)> on_message_raw;
    std::function<void(const ConnId& conn_id, const UserId& user_id)> on_auth;
};

class WebSocketServer {
   public:
    WebSocketServer(core::IApp& app, app::Dispatcher& dispatcher, ConnectionManager& conns,
                    ClientGateway& gateway, app::services::HubPublisher* hub_publisher = nullptr,
                    OriginAllowlist origins = {}, WsLimits limits = {});
    ~WebSocketServer();

    void wire(const std::string& pattern = "/ws");
    void shutdown();

    void set_hooks(WsHooks hooks) { hooks_ = std::move(hooks); }

   private:
    static std::string make_conn_id(void* p);

    core::IApp& app_;
    app::Dispatcher& dispatcher_;
    ConnectionManager& conns_;
    ClientGateway& gateway_;
    app::services::HubPublisher* hub_publisher_;
    OriginAllowlist origins_;
    WsLimits limits_;
    WsHooks hooks_{};
    Heartbeat heartbeat_;
    std::unique_ptr<infra::security::validation::MessageValidator> validator_;
};

}  // namespace net

#endif  // NET_WEBSOCKETSERVER_H
