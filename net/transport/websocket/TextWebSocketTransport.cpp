#include "net/transport/websocket/TextWebSocketTransport.h"

#include "net/transport/websocket/WsAppFactory.h"
#include "net/transport/websocket/utils.h"

#include <cctype>
#include <chrono>
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
      out_worker_(*app_, conns_, outgoing_queue) {
    auto verifier = infra::security::token::SupabaseJWTVerifier::create();
    if (!verifier.has_value()) {
        log(utils::LogLevel::ERROR, "Failed to initialize SupabaseJWTVerifier. Error: ",
            static_cast<int>(verifier.error()));
        return;
    }
    auth_.emplace(std::move(verifier.value()));
}

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
            log(utils::LogLevel::INFO, "Defer close uWS app loop from stop()");
            app_->uws().close();
        });
    } else if (app_) {
        // Loop was not captured; attempt a direct close on the app.
        log(utils::LogLevel::INFO, "Closing uWS app directly from stop()");
        app_->uws().close();
    }
}

const char* TextWSServer::name() const { return "TextWSServer"; }

void* TextWSServer::loop_id() const { return reinterpret_cast<void*>(app_->getUwsLoop()); }

void TextWSServer::set_hooks(const Hooks hooks) { hooks_ = hooks; }

void TextWSServer::wire() {
    app_->uws().ws<TextPerSocketData>(
        cfg_.ws_path,
        {
            .compression = uWS::SHARED_COMPRESSOR,
            .sendPingsAutomatically = false,
            .upgrade =
                [this](auto* res, auto* req, auto* ctx) {
                    std::string origin = std::string(req->getHeader("origin"));
                    // if (!origins_.is_allowed(origin)) {
                    //     res->writeStatus("403 Forbidden")->end("Origin not allowed");
                    //     return;
                    // }

                    if (!auth_.has_value()) {
                        res->writeStatus("500")->end("Server misconfiguration");
                        return;
                    }

                    if (active_connections_.load(std::memory_order_relaxed) >=
                        limits_.max_connections) {
                        res->writeStatus("503")->end("Max connections reached");
                        return;
                    }

                    const auto protocols = req->getHeader("sec-websocket-protocol");
                    const auto ws_key = req->getHeader("sec-websocket-key");
                    const auto token = extract_token(protocols);

                    if (token.empty()) {
                        res->writeStatus("401")->end("Unauthorized");
                        return;
                    }
                    auto auth_result = auth_->verify_token(std::string(token));
                    if (!auth_result.has_value()) {
                        res->writeStatus("401")->end("Unauthorized");
                        return;
                    }

                    const auto& claims = auth_result.value();
                    TextPerSocketData psd{};
                    psd.conn_id = conn_id_gen_.allocate();
                    psd.user_id = UserId{claims.id};
                    psd.role = claims.role;
                    psd.exp = claims.exp;
                    res->template upgrade<TextPerSocketData>(
                        std::move(psd), ws_key, "supabase",
                        req->getHeader("sec-websocket-extensions"), ctx);

                    active_connections_.fetch_add(1, std::memory_order_relaxed);
                },

            .open =
                [this](UwsSocket* ws) {
                    const auto* psd = ws->getUserData();
                    if (!psd) return;

                    if (psd->exp < 0) {
                        ws->end(4403, "Invalid token expiration");
                        return;
                    }

                    connection::ConnectionContext ctx(psd->conn_id, transport::WsHandle{ws},
                                                      TransportKind::TextWebSocket);
                    ctx.auth.is_authenticated = true;
                    ctx.auth.expires_at =
                        std::chrono::system_clock::time_point{std::chrono::seconds{psd->exp}};

                    conns_.attach(psd->conn_id, std::move(ctx));
                    heartbeat_service_.on_open(psd->conn_id);
                    hooks_.on_open(psd->conn_id, psd->user_id);
                },
            .message =
                [this](UwsSocket* ws, std::string_view data, uWS::OpCode op) {
                    if (op != uWS::OpCode::BINARY) return;
                    auto* psd = ws->getUserData();
                    if (!psd) return;

                    sercom::protocol::Envelope env;
                    if (!env.ParseFromArray(data.data(), data.size())) {
                        ws->end(1002, "Invalid envelope");
                        return;
                    }

                    if (env.version() != 1) {
                        ws->end(1002, "Protocol version mismatch");
                        return;
                    }

                    // FAST-PATH: application-level PING
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
                    conns_.detach(conn_id);

                    hooks_.on_close(conn_id, code, std::string(reason));
                    active_connections_.fetch_sub(1, std::memory_order_relaxed);
                },
        });

    heartbeat_service_.start();
    out_worker_.start();
}

}  // namespace net::transport::websocket
