// .cpp
#include "net/WebSocketServer.h"

#include <nlohmann/json.hpp>

using nlohmann::json;

namespace net {

WebSocketServer::WebSocketServer(core::IApp& app, app::Dispatcher& dispatcher,
                                 ConnectionManager& conns, ClientGateway& gateway,
                                 OriginAllowlist origins, WsLimits limits)
    : app_(app),
      dispatcher_(dispatcher),
      conns_(conns),
      gateway_(gateway),
      origins_(std::move(origins)),
      limits_(limits) {}

void WebSocketServer::wire(const std::string& pattern) {
    // Let dispatcher update PSD on successful auth
    dispatcher_.on_auth_success([this](const ConnId& conn_id, const UserId& user_id) {
        if (hooks_.on_auth) hooks_.on_auth(conn_id, user_id);
    });

    app_.uws().ws<PerSocketData>(
        pattern,
        {
            .upgrade =
                [this](auto* res, auto* req, auto* ctx) {
                    std::string origin = std::string(req->getHeader("origin"));
                    if (!origins_.is_allowed(origin)) {
                        res->writeStatus("403 Forbidden")->end("Origin not allowed");
                        return;
                    }
                    res->template upgrade<PerSocketData>({}, req->getHeader("sec-websocket-key"),
                                                         req->getHeader("sec-websocket-protocol"),
                                                         req->getHeader("sec-websocket-extensions"),
                                                         ctx);
                },

            .open =
                [this](UwsSocket* ws) {
                    auto* psd = ws->getUserData();
                    psd->conn_id = make_conn_id(ws);
                    psd->connected_at = std::chrono::system_clock::now();
                    conns_.attach(psd->conn_id, ws);

                    json hello = {{"type", "hello"}, {"conn_id", psd->conn_id.value}};
                    gateway_.send_now(psd->conn_id, hello);
                },
            .message =
                [this](UwsSocket* ws, std::string_view data, OpCode op) {
                    if (op != OpCode::TEXT) return;
                    if (hooks_.on_message_raw) {
                        auto* psd = ws->getUserData();
                        hooks_.on_message_raw(psd->conn_id, data);
                    }
                    if (data.size() > limits_.max_message_bytes) {
                        ws->send(R"({"type":"error","code":"payload_too_large"})", OpCode::TEXT);
                        return;
                    }
                    json j;
                    try {
                        j = json::parse(data);
                    } catch (...) {
                        ws->send(R"({"type":"error","code":"bad_json"})", OpCode::TEXT);
                        return;
                    }
                    auto type = j.value("type", "");
                    if (type.empty()) {
                        ws->send(R"({"type":"error","code":"missing_type"})", OpCode::TEXT);
                        return;
                    }
                    if (type == "ping") {
                        ws->send(R"({"type":"pong"})", OpCode::TEXT);
                        return;
                    }

                    auto* psd = ws->getUserData();
                    app::CommandContext ctx{*psd};
                    ctx.input.data = j;
                    ctx.input.received_at = std::chrono::system_clock::now();

                    dispatcher_.dispatch(type, ctx);

                    if (ctx.output.success) {
                        gateway_.send_now(psd->conn_id, ctx.output.data);
                    }
                },
            .drain =
                [this](UwsSocket* /*ws*/) {
                    // backpressure hook (optional)
                },
                
            .close =
                [this](UwsSocket* ws, int code, std::string_view reason) {
                    auto* psd = ws->getUserData();
                    if (psd) {
                        conns_.detach(psd->conn_id);
                        if (hooks_.on_close) hooks_.on_close(psd->conn_id, code, reason);
                    }
                },
        });
}

std::string WebSocketServer::make_conn_id(void* p) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%p", p);
    return std::string(buf);
}

}  // namespace net
