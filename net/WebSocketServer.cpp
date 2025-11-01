// .cpp
#include "net/WebSocketServer.h"

#include <nlohmann/json.hpp>

using nlohmann::json;
using UwsSocket = UwsWebSocketT<PerSocketData>;

namespace net {

WebSocketServer::WebSocketServer(IApp& app, app::Dispatcher& dispatcher, ConnectionManager& conns,
                                 OriginAllowlist origins, WsLimits limits)
    : app_(app),
      dispatcher_(dispatcher),
      conns_(conns),
      origins_(std::move(origins)),
      limits_(limits) {}

void WebSocketServer::wire(const std::string& pattern) {
    // Let dispatcher update PSD on successful auth
    dispatcher_.on_auth_success([this](const ConnId& conn_id, const UserId& user_id) {
        if (auto* ws = conns_.get(conn_id)) {
            if (auto* psd = ws->getUserData()) {
                psd->user_id = user_id;
                psd->authenticated = true;
                if (hooks_.on_auth) hooks_.on_auth(conn_id, user_id);
            }
        }
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
                    UwsWSAdapter adapter(ws);
                    auto* psd = adapter.getUserData();
                    psd->conn_id = make_conn_id(ws);
                    psd->connected_at = std::chrono::system_clock::now();

                    // Attach this connection
                    conns_.attach(psd->conn_id, &adapter);  // store adapter pointer wrapper

                    if (hooks_.on_open) hooks_.on_open(psd->conn_id);

                    // Optional hello
                    json hello = {{"type", "hello"}, {"conn_id", psd->conn_id.value}};
                    adapter.send(hello.dump(), OpCode::TEXT);
                },

            .message =
                [this](UwsSocket* ws, std::string_view data, OpCode op) {
                    if (op != OpCode::TEXT) return;
                    if (data.size() > limits_.max_message_bytes) {
                        ws->send(R"({"type":"error","code":"payload_too_large"})", OpCode::TEXT);
                        return;
                    }

                    if (hooks_.on_message_raw) {
                        auto* psd = ws->getUserData();
                        hooks_.on_message_raw(psd ? psd->conn_id : ConnId{""}, data);
                    }

                    // Parse
                    json j;
                    try {
                        j = json::parse(data);
                    } catch (...) {
                        ws->send(R"({"type":"error","code":"bad_json"})", OpCode::TEXT);
                        return;
                    }

                    const std::string type = j.value("type", "");
                    if (type.empty()) {
                        ws->send(R"({"type":"error","code":"missing_type"})", OpCode::TEXT);
                        return;
                    }

                    // Ping/pong short-circuit
                    if (type == "ping") {
                        ws->send(R"({"type":"pong"})", OpCode::TEXT);
                        return;
                    }

                    // Build context
                    UwsWSAdapter adapter(ws);
                    auto* psd = adapter.getUserData();

                    CommandContext ctx;
                    ctx.conn_id = psd ? psd->conn_id : ConnId{""};
                    ctx.user_id = psd ? psd->user_id : UserId{""};
                    ctx.current_hub = psd ? psd->current_hub_id : HubId{""};
                    ctx.current_chan = psd ? psd->current_channel_id : ChannelId{""};
                    ctx.remote_ip = adapter.remote_address();
                    ctx.received_at = std::chrono::system_clock::now();

                    // Dispatch
                    if (auto res = dispatcher_.dispatch(type, ctx, j)) {
                        adapter.send(res->dump(), OpCode::TEXT);
                    }
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
