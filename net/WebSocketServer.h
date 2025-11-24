#ifndef NET_WEBSOCKETSERVER_H
#define NET_WEBSOCKETSERVER_H

#include "app/queue/ThreadSafeCmdQueue.h"
#include "core/IApp.h"
#include "core/Types.h"
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
    WebSocketServer(core::IApp& app, ConnectionManager& conns, ClientGateway& gateway,
                    OriginAllowlist origins = {}, WsLimits limits = {});
    ~WebSocketServer();

    void wire(const std::string& pattern = "/ws");
    void shutdown();

    void set_hooks(WsHooks hooks) { hooks_ = std::move(hooks); }

   private:
    static std::string make_conn_id(void* p);

    core::IApp& app_;
    ConnectionManager& conns_;
    ClientGateway& gateway_;
    OriginAllowlist origins_;
    WsLimits limits_;
    WsHooks hooks_{};
    Heartbeat heartbeat_;
};

}  // namespace net

#endif  // NET_WEBSOCKETSERVER_H
