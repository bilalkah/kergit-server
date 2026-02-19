#include "net/transport/websocket/WebSocketTransport.h"

#include "net/transport/websocket/WsAppFactory.h"
#include "net/transport/websocket/utils.h"
#include "utils/EventLogger.h"
#include "utils/Metrics.h"

#include <cctype>
#include <string_view>

namespace net::transport::websocket {

TextWSServer::TextWSServer(core::NetworkStackConfig cfg, connection::ConnectionRegistery& conns,
                           outbound::OutgoingQueue& outgoing_queue, OriginAllowlist origins,
                           WsLimits limits)
    : cfg_(std::move(cfg)),
      origins_(std::move(origins)),
      limits_(std::move(limits)),
      conns_(conns),
      app_(AppFactory::create(cfg_)),
      heartbeat_service_(*app_, conns_),
      out_worker_(*app_, conns_, *this, outgoing_queue) {}

TextWSServer::~TextWSServer() {}

void TextWSServer::start() {
    wire();

    loop_.store(app_->uws().getLoop(), std::memory_order_release);

    app_->uws().listen(cfg_.host, cfg_.port, [&](auto* token) {
        if (token) {
            log(utils::LogLevel::INFO, "Listener bound successfully.");
            listen_token_ = token;
            started_.store(true, std::memory_order_release);
        } else {
            log(utils::LogLevel::ERROR, "Listener failed to bind.");
            started_.store(false, std::memory_order_release);
        }
    });
    app_->uws().run();
}

void TextWSServer::stop() {
    if (stop_requested_.exchange(true)) return;

    heartbeat_service_.stop();
    out_worker_.stop();

    if (auto* loop = loop_.load(std::memory_order_acquire)) {
        loop->defer([this]() {
            log(utils::LogLevel::WARN, "Defer close uWS app loop from stop()");
            app_->uws().close();
        });
    } else if (app_) {
        log(utils::LogLevel::WARN, "Closing uWS app directly from stop()");
        app_->uws().close();
    }
}

const char* TextWSServer::name() const { return "TextWSServer"; }

void* TextWSServer::loop_id() const { return reinterpret_cast<void*>(app_->getUwsLoop()); }

void TextWSServer::set_hooks(const Hooks hooks) { hooks_ = hooks; }

bool TextWSServer::send(transport::WsHandle& handle, std::string_view payload,
                        bool binary) noexcept {
    if (!handle.valid()) return false;
    const auto status = handle.send(payload, binary);
    return status == transport::websocket::UwsSocket::SendStatus::SUCCESS;
}

bool TextWSServer::is_backpressured(const transport::WsHandle& handle) const noexcept {
    static constexpr std::size_t kBackpressureHighWatermark = 512 * 1024;
    if (!handle.valid()) return false;
    return handle.ws->getBufferedAmount() >= kBackpressureHighWatermark;
}

void TextWSServer::wire() {
    app_->uws().ws<PerSocketData>(
        cfg_.ws_path,
        {
            .compression = uWS::SHARED_COMPRESSOR,
            .sendPingsAutomatically = false,
            .upgrade =
                [this](auto* res, auto* req, auto* ctx) {
                    std::string origin = std::string(req->getHeader("origin"));
                    if (!origins_.is_allowed(origin)) {
                        res->writeStatus("403")->end("Origin not allowed");
                        log(utils::LogLevel::ERROR, "Origin not allowed: " + origin);
                        return;
                    }
                    if (active_connections_.load(std::memory_order_relaxed) >=
                        limits_.max_connections) {
                        res->writeStatus("503")->end("Max connections reached");
                        log(utils::LogLevel::ERROR,
                            "Max connections reached. Active connections: " +
                                std::to_string(
                                    active_connections_.load(std::memory_order_relaxed)));
                        return;
                    }
                    const auto ws_key = req->getHeader("sec-websocket-key");
                    PerSocketData psd{};
                    psd.conn_id = conn_id_gen_.allocate();

                    // Capture conn_id before moving psd
                    auto conn_id_str = psd.conn_id.value;

                    res->template upgrade<PerSocketData>(std::move(psd), ws_key, "",
                                                         req->getHeader("sec-websocket-extensions"),
                                                         ctx);

                    utils::EventLogger::instance().log(
                        utils::EventCategory::SESSION, "", "WS_UPGRADE", 0,
                        "netstack:" + std::to_string(cfg_.port_index) + ",conn:" + conn_id_str);
                    active_connections_.fetch_add(1, std::memory_order_relaxed);
                },

            .open =
                [this](UwsSocket* ws) {
                    const auto* psd = ws->getUserData();
                    if (!psd) return;

                    utils::EventLogger::instance().log(
                        utils::EventCategory::SESSION, "", "WS_OPEN", 0,
                        "netstack:" + std::to_string(cfg_.port_index) + ",conn:" + psd->conn_id.value);

                    connection::ConnectionContext ctx(psd->conn_id, transport::WsHandle{ws},
                                                      TransportKind::TextWebSocket,
                                                      cfg_.port_index);
                    ctx.auth_state = connection::AuthState::AUTH_PENDING;
                    conns_.attach(psd->conn_id, std::move(ctx));
                    heartbeat_service_.on_open(psd->conn_id);
                    // Note: on_open not called here - bootstrap is triggered after auth
                    // via AuthenticateCommand pushing ConnectionEvent

                    utils::metrics::counters().active_connections.fetch_add(
                        1, std::memory_order_relaxed);
                },
            .message =
                [this](UwsSocket* ws, std::string_view data, uWS::OpCode op) {
                    if (op != uWS::OpCode::BINARY) return;
                    auto* psd = ws->getUserData();
                    if (!psd) return;

                    sercom::protocol::Envelope env;
                    if (!env.ParseFromArray(data.data(), data.size())) {
                        ws->end(1002, "Invalid envelope");
                        log(utils::LogLevel::ERROR, "Invalid envelope");
                        return;
                    }

                    if (env.version() != 1) {
                        ws->end(1002, "Protocol version mismatch");
                        log(utils::LogLevel::ERROR, "Protocol version mismatch");
                        return;
                    }

                    auto conn_view = conns_.get_view(psd->conn_id);
                    if (!conn_view.has_value()) {
                        ws->end(4401, "connection_not_found");
                        return;
                    }

                    if (conn_view->auth_state == connection::AuthState::AUTH_FAILED) {
                        ws->end(4401, "auth_failed");
                        return;
                    }

                    if (conn_view->auth_state != connection::AuthState::AUTHENTICATED &&
                        env.type() != sercom::protocol::Envelope::AUTH) {
                        // Strict AUTH-first policy in socket layer: drop everything except AUTH
                        // until the connection becomes authenticated.
                        return;
                    }

                    // FAST-PATH: application-level PING (only after auth)
                    if (env.type() == sercom::protocol::Envelope::PING) {
                        auto res = make_app_pong_response(env);
                        if (!res.has_value()) {
                            ws->end(1002, res.error());
                            return;
                        }
                        ws->send(res.value(), uWS::OpCode::BINARY);
                        return;
                    }

                    hooks_.on_message(psd->conn_id, std::move(env));
                },
            .drain =
                [this](UwsSocket* ws) {
                    auto* psd = ws->getUserData();
                    if (!psd) return;
                    auto conn = conns_.get(psd->conn_id);
                    if (!conn.has_value()) return;
                    auto& conn_ctx = conn.value();

                    while (!conn_ctx.pending.empty()) {
                        const auto& msg = conn_ctx.pending.front();
                        const auto status = ws->send(msg.first, msg.second);
                        if (status != transport::websocket::UwsSocket::SendStatus::SUCCESS) {
                            break;
                        }
                        conn_ctx.pending.pop_front();
                    }
                },

            .pong =
                [this](UwsSocket* ws, std::string_view data) {
                    auto* psd = ws->getUserData();
                    if (!psd) return;

                    const auto& status = heartbeat_service_.on_pong(psd->conn_id);

                    if (!status.has_value()) return;
                },
            .close =
                [this](UwsSocket* ws, int code, std::string_view reason) {
                    auto* psd = ws->getUserData();
                    if (!psd) return;

                    auto conn_id = psd->conn_id;

                    // Get user_id from connection context before detaching
                    std::string user_id_str = "";
                    auto conn = conns_.get(conn_id);
                    if (conn.has_value() && conn->user_id.has_value()) {
                        user_id_str = conn->user_id->value;
                    }

                    utils::EventLogger::instance().log(
                        utils::EventCategory::SESSION, user_id_str, "WS_CLOSE", 0,
                        "conn:" + conn_id.value + " code:" + std::to_string(code));

                    conns_.detach(conn_id);

                    hooks_.on_close(conn_id, code, std::string(reason));
                    active_connections_.fetch_sub(1, std::memory_order_relaxed);
                    utils::metrics::counters().active_connections.fetch_sub(
                        1, std::memory_order_relaxed);
                },
        });

    heartbeat_service_.start();
    out_worker_.start();
}

}  // namespace net::transport::websocket
