#ifndef NET_TRANSPORT_WEBSOCKET_WebSocketTransport_H
#define NET_TRANSPORT_WEBSOCKET_WebSocketTransport_H

#include "core/ServerConfig.h"
#include "infra/security/token/SupabaseJwtVerifier.h"
#include "infra/security/token/SupabaseJwtVerifierJwtCpp.h"
#include "net/connection/ConnectionRegistery.h"
#include "net/outbound/OutgoingQueue.h"
#include "net/outbound/OutgoingWorker.h"
#include "net/runtime/HeartbeatService.h"
#include "net/transport/IOutboundTransport.h"
#include "net/transport/ITransport.h"
#include "net/transport/websocket/IWsApp.h"
#include "net/transport/websocket/PerSocketData.h"
#include "net/transport/websocket/UwsTypes.h"
#include "utils/Loggable.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace app::queue {
class IEventSink;
}

namespace net::outbound {
class OutboundFlushEngine;
}

namespace net::transport::websocket {

struct WsLimits {
    size_t max_message_bytes = 256 * 1024;
    uint8_t max_connections = 255;
};

struct OriginAllowlist {
    bool is_allowed(const std::string& origin) const {
        if (origin.empty()) return true;

        return origin.find("http://localhost") == 0 || origin.find("https://localhost") == 0 ||
               origin.find("http://127.0.0.1") == 0 || origin.find("https://127.0.0.1") == 0 ||
               origin.find("http://192.168.") == 0 || origin.find("https://192.168.") == 0;
    }
};

class TextWSServer : public ITransportServer,
                     public transport::IOutboundTransport,
                     public utils::Loggable {
   public:
    explicit TextWSServer(core::NetworkStackConfig cfg, connection::ConnectionRegistery& conns,
                          outbound::OutgoingQueue& outgoing_queue, OriginAllowlist origins = {},
                          WsLimits limits = {});
    ~TextWSServer();

    void start() override;
    void stop() override;

    bool is_started() const override { return started_; }
    bool is_stopped() const override { return stopped_; }

    const char* name() const override;
    void* loop_id() const override;
    void set_hooks(Hooks hooks) override;
    bool send(transport::WsHandle& handle, std::string_view payload, bool binary) noexcept override;
    bool is_backpressured(const transport::WsHandle& handle) const noexcept override;

   private:
    class ConnIdGenerator {
       public:
        ConnId allocate() {
            uint64_t id = next_.fetch_add(1, std::memory_order_relaxed);
            return ConnId{std::to_string(id)};
        }

       private:
        std::atomic<uint64_t> next_{1};
    };
    void wire();
    /**
     * Configuration
     */
    core::NetworkStackConfig cfg_{};
    OriginAllowlist origins_{};
    WsLimits limits_{};

    /**
     * References
     */
    connection::ConnectionRegistery& conns_;

    /**
     * Components
     */
    std::unique_ptr<IApp> app_;
    ::us_listen_socket_t* listen_token_{nullptr};
    std::atomic<uWS::Loop*> loop_{nullptr};
    runtime::HeartbeatService heartbeat_service_;
    outbound::OutgoingWorker out_worker_;
    Hooks hooks_{};
    std::optional<infra::security::token::SupabaseJWTVerifier> auth_{};
    std::optional<infra::security::token::SupabaseJWTVerifierJwtCpp> auth_jwt_cpp_{};
    ConnIdGenerator conn_id_gen_{};

    /**
     * State
     */
    std::atomic<bool> started_{false};
    std::atomic<bool> stopped_{false};
    std::atomic<bool> stop_requested_{false};

    /**
     * Number of active connections
     */
    std::atomic<uint8_t> active_connections_{0};
    std::unique_ptr<outbound::OutboundFlushEngine> flush_engine_;
};

}  // namespace net::transport::websocket

#endif  // NET_TRANSPORT_WEBSOCKET_WebSocketTransport_H
