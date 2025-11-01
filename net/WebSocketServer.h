#ifndef NET_WEBSOCKETSERVER_H
#define NET_WEBSOCKETSERVER_H

#include "app/Dispatcher.h"
#include "core/IApp.h"
#include "core/Types.h"
#include "net/ConnectionManager.h"
#include "net/PerSocketData.h"
#include "net/WSAdapter.h"

#include <functional>
#include <memory>
#include <string>

namespace net {

struct WsLimits {
    size_t max_message_bytes = 256 * 1024;
};

struct OriginAllowlist {
    // Very simple; replace with your util if you already have one
    bool is_allowed(const std::string& origin) const {
        if (origin.empty()) return true;  // allow no-origin (native apps)
        // allow localhost/dev tools by default
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
    WebSocketServer(IApp& app, app::Dispatcher& dispatcher, ConnectionManager& conns,
                    OriginAllowlist origins = {}, WsLimits limits = {});

    void wire(const std::string& pattern);

    void set_hooks(WsHooks hooks) { hooks_ = std::move(hooks); }

   private:
    static std::string make_conn_id(void* p);

    IApp& app_;
    app::Dispatcher& dispatcher_;
    ConnectionManager& conns_;
    OriginAllowlist origins_;
    WsLimits limits_;
    WsHooks hooks_{};
};

}  // namespace net

#endif  // NET_WEBSOCKETSERVER_H
