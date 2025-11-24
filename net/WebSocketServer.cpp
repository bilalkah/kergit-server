// .cpp
#include "net/WebSocketServer.h"

#include <chrono>
#include <nlohmann/json.hpp>
#include <unordered_set>

using nlohmann::json;
namespace {

std::string safe_display(const net::PerSocketData& psd) {
    if (!psd.username.empty()) return psd.username;
    return "Member";
}
}  // namespace
namespace net {

WebSocketServer::WebSocketServer(core::IApp& app, ConnectionManager& conns, ClientGateway& gateway,
                                 EventQueue& in_q, OutgoingQueue& out_q, OriginAllowlist origins,
                                 WsLimits limits)
    : app_(app),
      conns_(conns),
      gateway_(gateway),
      in_q_(in_q),
      origins_(std::move(origins)),
      limits_(limits),
      heartbeat_(app, conns),
      out_consumer_(app, conns, gateway, out_q) {}

WebSocketServer::~WebSocketServer() { shutdown(); }

void WebSocketServer::wire(const std::string& pattern) {
    app_.uws().ws<PerSocketData>(
        pattern, {
                     .sendPingsAutomatically = false,
                     .upgrade =
                         [this](auto* res, auto* req, auto* ctx) {
                             std::string origin = std::string(req->getHeader("origin"));
                             if (!origins_.is_allowed(origin)) {
                                 res->writeStatus("403 Forbidden")->end("Origin not allowed");
                                 return;
                             }
                             res->template upgrade<PerSocketData>(
                                 {}, req->getHeader("sec-websocket-key"),
                                 req->getHeader("sec-websocket-protocol"),
                                 req->getHeader("sec-websocket-extensions"), ctx);
                         },

                     .open =
                         [this](UwsSocket* ws) {
                             auto* psd = ws->getUserData();
                             psd->conn_id = make_conn_id(ws);
                             psd->connected_at = std::chrono::system_clock::now();
                             heartbeat_.on_open(*psd);
                             conns_.attach(psd->conn_id, ws);

                             gateway_.say_hello(psd->conn_id);
                         },
                     .message =
                         [this](UwsSocket* ws, std::string_view data, OpCode op) {
                             if (op != OpCode::TEXT) return;
                             auto* psd = ws->getUserData();

                             CommandRequest req;
                             req.payload = std::string{data};

                             req.conn_id = psd->conn_id;
                             req.user_id = psd->user_id;
                             req.current_hub_id = psd->current_hub_id;
                             req.current_channel_id = psd->current_channel_id;
                             req.authenticated = psd->authenticated;
                             req.received_at = std::chrono::system_clock::now();
                             in_q_.push(Event{req});

                             if (hooks_.on_message_raw) {
                                 hooks_.on_message_raw(psd->conn_id, data);
                             }
                         },
                     .drain =
                         [](UwsSocket* /*ws*/) {
                             // No-op for now
                         },

                     .pong =
                         [this](auto* ws, std::string_view data) {
                             auto& psd = *static_cast<PerSocketData*>(ws->getUserData());
                             heartbeat_.on_pong(psd);

                             // Optimization: send connection status on pong
                             ws->send(heartbeat_.conn_status_message(), uWS::OpCode::TEXT);
                         },
                     .close =
                         [this](UwsSocket* ws, int code, std::string_view reason) {
                             auto* psd = ws->getUserData();
                             if (!psd) return;

                             auto conn_id = psd->conn_id;
                             auto user_id = psd->user_id;
                             auto snap = psd->snapshot;
                             auto display = safe_display(*psd);

                             gateway_.unsubscribe_all(conn_id);
                             conns_.detach(conn_id);

                             psd->authenticated = false;
                             psd->snapshot.reset();  // or clear local fields

                             if (hooks_.on_close) hooks_.on_close(conn_id, code, reason);

                             // ---- Notify workers/app ----
                             DisconnectEvent ev{conn_id, user_id, snap, display};
                             ev.code = code;
                             ev.reason = std::string(reason);
                             in_q_.push(Event{ev});
                         },
                 });

    heartbeat_.start();
}

void WebSocketServer::shutdown() { heartbeat_.stop(); }

std::string WebSocketServer::make_conn_id(void* p) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%p", p);
    return std::string(buf);
}

}  // namespace net
