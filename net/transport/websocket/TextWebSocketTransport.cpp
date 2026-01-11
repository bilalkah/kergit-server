#include "net/transport/websocket/TextWebSocketTransport.h"

#include "net/transport/websocket/WsAppFactory.h"
#include "net/transport/websocket/utils.h"
#include "proto/auth/auth.pb.h"

#include <cctype>
#include <chrono>
#include <string_view>

namespace net::transport::websocket {
namespace {

std::string_view jwt_error_code(infra::security::token::JwtVerifyError err) {
    using infra::security::token::JwtVerifyError;
    switch (err) {
        case JwtVerifyError::EmptyToken:
            return "EMPTY_TOKEN";
        case JwtVerifyError::InvalidFormat:
            return "INVALID_FORMAT";
        case JwtVerifyError::UnsupportedAlgorithm:
            return "UNSUPPORTED_ALG";
        case JwtVerifyError::InvalidSignature:
            return "INVALID_SIGNATURE";
        case JwtVerifyError::TokenExpired:
            return "TOKEN_EXPIRED";
        case JwtVerifyError::MissingClaims:
            return "MISSING_CLAIMS";
        case JwtVerifyError::KeyNotFound:
            return "KEY_NOT_FOUND";
        case JwtVerifyError::JwkParseError:
            return "JWK_PARSE_ERROR";
    }
    return "UNKNOWN_ERROR";
}

std::string_view jwt_error_message(infra::security::token::JwtVerifyError err) {
    using infra::security::token::JwtVerifyError;
    switch (err) {
        case JwtVerifyError::EmptyToken:
            return "Token is empty";
        case JwtVerifyError::InvalidFormat:
            return "Token format is invalid";
        case JwtVerifyError::UnsupportedAlgorithm:
            return "Token algorithm is unsupported";
        case JwtVerifyError::InvalidSignature:
            return "Token signature is invalid";
        case JwtVerifyError::TokenExpired:
            return "Token has expired";
        case JwtVerifyError::MissingClaims:
            return "Token claims are missing";
        case JwtVerifyError::KeyNotFound:
            return "Signing key not found";
        case JwtVerifyError::JwkParseError:
            return "Failed to parse JWK";
    }
    return "Unknown token error";
}

std::string build_auth_envelope(sercom::protocol::auth::AuthStatus status,
                                std::string_view code, std::string_view message) {
    sercom::protocol::auth::AuthResponse resp;
    resp.set_status(status);
    if (status == sercom::protocol::auth::AUTH_STATUS_SUCCESS) {
        resp.mutable_success();
    } else {
        auto* failure = resp.mutable_failure();
        failure->set_code(std::string(code));
        failure->set_message(std::string(message));
    }

    std::string payload;
    if (!resp.SerializeToString(&payload)) {
        return {};
    }

    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(sercom::protocol::Envelope::AUTH);
    env.set_payload(std::move(payload));

    std::string out_bytes;
    if (!env.SerializeToString(&out_bytes)) {
        return {};
    }
    return out_bytes;
}


}  // namespace

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
                    if (!origins_.is_allowed(origin)) {
                        res->writeStatus("403 Forbidden")->end("Origin not allowed");
                        return;
                    }
                    if (!auth_.has_value()) {
                        res->writeStatus("500")->end();
                        return;
                    }
                    if (active_connections_.load(std::memory_order_relaxed) >=
                        limits_.max_connections) {
                        res->writeStatus("503")->end("Max connections reached");
                        return;
                    }
                    auto protocols = req->getHeader("sec-websocket-protocol");
                    auto ws_key = req->getHeader("sec-websocket-key");
                    auto token = extract_token(protocols);
                    if (token.empty()) {
                        res->writeStatus("401")->end();
                        return;
                    }
                    auto auth_result = auth_->verify_token(std::string(token));
                    if (!auth_result.has_value()) {
                        res->writeStatus("401")->end();
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
                    auto* psd = ws->getUserData();
                    connection::ConnectionContext ctx(psd->conn_id,
                                                      transport::WsHandle{ws},
                                                      TransportKind::TextWebSocket);
                    ctx.auth.is_authenticated = true;
                    if (psd->exp > 0) {
                        ctx.auth.expires_at =
                            std::chrono::system_clock::time_point{std::chrono::seconds{psd->exp}};
                    } else {
                        ws->end(4403, "Invalid token expiration");
                        return;
                    }

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

                    if (env.type() == sercom::protocol::Envelope::AUTH) {
                        auto send_auth_response =
                            [&](sercom::protocol::auth::AuthStatus status,
                                std::string_view code, std::string_view message) {
                                auto out = build_auth_envelope(status, code, message);
                                if (!out.empty()) {
                                    ws->send(out, uWS::OpCode::BINARY);
                                }
                            };

                        sercom::protocol::auth::AuthRequest req;
                        if (!req.ParseFromArray(env.payload().data(), env.payload().size())) {
                            send_auth_response(sercom::protocol::auth::AUTH_STATUS_FAILED,
                                               "INVALID_REQUEST", "Invalid auth payload");
                            return;
                        }

                        if (req.type() != sercom::protocol::auth::AUTH_TYPE_REAUTH) {
                            send_auth_response(sercom::protocol::auth::AUTH_STATUS_FAILED,
                                               "UNSUPPORTED_AUTH_TYPE", "Only REAUTH is supported");
                            return;
                        }

                        if (req.provider() != sercom::protocol::auth::AUTH_PROVIDER_SUPABASE) {
                            send_auth_response(sercom::protocol::auth::AUTH_STATUS_FAILED,
                                               "UNSUPPORTED_PROVIDER",
                                               "Unsupported auth provider");
                            return;
                        }

                        if (req.token().empty()) {
                            send_auth_response(sercom::protocol::auth::AUTH_STATUS_FAILED,
                                               "EMPTY_TOKEN", "Token is required");
                            return;
                        }

                        if (!auth_.has_value()) {
                            send_auth_response(sercom::protocol::auth::AUTH_STATUS_FAILED,
                                               "AUTH_UNAVAILABLE",
                                               "Auth verifier is not initialized");
                            return;
                        }

                        auto auth_result = auth_->verify_token(req.token());
                        if (!auth_result.has_value()) {
                            send_auth_response(sercom::protocol::auth::AUTH_STATUS_FAILED,
                                               jwt_error_code(auth_result.error()),
                                               jwt_error_message(auth_result.error()));
                            return;
                        }

                        const auto& claims = auth_result.value();
                        if (claims.id != psd->user_id.value) {
                            send_auth_response(sercom::protocol::auth::AUTH_STATUS_FAILED,
                                               "USER_MISMATCH",
                                               "Token user does not match connection");
                            ws->end(4401, "reauth_user_mismatch");
                            return;
                        }

                        if (claims.exp <= 0) {
                            send_auth_response(sercom::protocol::auth::AUTH_STATUS_FAILED,
                                               "MISSING_EXP", "Token expiration missing");
                            return;
                        }

                        auto update_result = conns_.mutate(
                            psd->conn_id, [&](connection::ConnectionContext& ctx) {
                                ctx.auth.is_authenticated = true;
                                ctx.auth.expires_at =
                                    std::chrono::system_clock::time_point{
                                        std::chrono::seconds{claims.exp}};
                            });
                        if (!update_result.has_value()) {
                            send_auth_response(sercom::protocol::auth::AUTH_STATUS_FAILED,
                                               "CONNECTION_NOT_FOUND",
                                               update_result.error().message);
                            return;
                        }

                        psd->exp = claims.exp;
                        send_auth_response(sercom::protocol::auth::AUTH_STATUS_SUCCESS, "", "");
                        return;
                    }

                    hooks_.on_message(psd->conn_id, data);
                },
            .drain =
                [](UwsSocket* /*ws*/) {
                    // No-op for now
                },

            .pong =
                [this](UwsSocket* ws, std::string_view data) {
                    auto* psd = ws->getUserData();
                    const auto& status = heartbeat_service_.on_pong(psd->conn_id);

                    if (!status.has_value()) return;
                },
            .close =
                [this](UwsSocket* ws, int code, std::string_view reason) {
                    auto* psd = ws->getUserData();
                    if (!psd) return;

                    log(utils::LogLevel::INFO, "Connection closed: ", psd->conn_id.value,
                        " Code: ", code, " Reason: ", reason);

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
