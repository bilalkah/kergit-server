// .cpp
#include "net/WebSocketServer.h"

#include <chrono>
#include <nlohmann/json.hpp>
#include <unordered_set>

using nlohmann::json;

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
                             conns_.attach(psd->conn_id, ws);
                             
                             ConnectEvent ev{psd->conn_id, psd->snapshot};
                             in_q_.push(Event{ev});
                             
                             heartbeat_.on_open(*psd);
                         },
                     .message =
                         [this](UwsSocket* ws, std::string_view data, OpCode op) {
                             if (op != OpCode::TEXT) return;
                             auto* psd = ws->getUserData();

                             CommandRequest req;
                             req.payload = std::string{data};

                             req.conn_id = psd->conn_id;
                             req.snapshot = psd->snapshot;
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
                             auto snap = psd->snapshot;
                             conns_.detach(conn_id);

                             if (hooks_.on_close) hooks_.on_close(conn_id, code, reason);

                             // ---- Notify workers/app ----
                             DisconnectEvent ev{conn_id, snap};
                             ev.code = code;
                             ev.reason = std::string(reason);
                             in_q_.push(Event{ev});
                         },
                 });

    heartbeat_.start();
    out_consumer_.start();
}

void WebSocketServer::shutdown() {
    heartbeat_.stop();
    out_consumer_.stop();
}

std::string WebSocketServer::make_conn_id(void* p) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%p", p);
    return std::string(buf);
}

}  // namespace net
