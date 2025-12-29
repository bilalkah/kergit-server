#include "net/transport/websocket/TextWebSocketTransport.h"

#include "net/transport/websocket/WsAppFactory.h"

#include <chrono>

namespace net::transport::websocket {

TextWSServer::TextWSServer(core::NetworkStackConfig cfg, connection::ConnectionRegistery& conns,
                           EventQueue& event_queue, outbound::OutgoingQueue& outgoing_queue,
                           OriginAllowlist origins, WsLimits limits)
    : cfg_(std::move(cfg)),
      origins_(std::move(origins)),
      limits_(std::move(limits)),
      conns_(conns),
      event_queue_(event_queue),
      app_(AppFactory::create(cfg_)),
      heartbeat_service_(*app_, conns_),
      out_worker_(*app_, conns_, outgoing_queue) {}

TextWSServer::~TextWSServer() { stop(); }

void TextWSServer::start() {
    wire();

    listen_token_ = nullptr;
    app_->uws().listen(cfg_.host, cfg_.port, [&](auto* token) {
        if (token) {
            log(utils::LogLevel::INFO, "Listener bound successfully.");
            listen_token_ = token;
        } else {
            log(utils::LogLevel::ERROR, "Listener failed to bind.");
        }
    });
    app_->uws().run();
}

void TextWSServer::stop() {
    heartbeat_service_.stop();
    out_worker_.stop();

    auto* loop = app_->getUwsLoop();
    if (loop) {
        loop->defer([this]() {
            log(utils::LogLevel::INFO, "Defer close uWS app loop from TextWSServer::shutdown()");
            app_->uws().close();
        });
    }
}

const char* TextWSServer::name() const { return "TextWSServer"; }

void* TextWSServer::loop_id() const { return reinterpret_cast<void*>(app_->getUwsLoop()); }

void TextWSServer::wire() {
    app_->uws().ws<TextPerSocketData>(
        cfg_.ws_path, {
                          .sendPingsAutomatically = false,
                          .upgrade =
                              [this](auto* res, auto* req, auto* ctx) {
                                  std::string origin = std::string(req->getHeader("origin"));
                                  if (!origins_.is_allowed(origin)) {
                                      res->writeStatus("403 Forbidden")->end("Origin not allowed");
                                      return;
                                  }
                                  res->template upgrade<TextPerSocketData>(
                                      {}, req->getHeader("sec-websocket-key"),
                                      req->getHeader("sec-websocket-protocol"),
                                      req->getHeader("sec-websocket-extensions"), ctx);
                              },

                          .open =
                              [this](UwsSocket* ws) {
                                  auto* psd = ws->getUserData();
                                  psd->conn_id = make_conn_id(ws);
                                  conns_.attach(
                                      psd->conn_id,
                                      connection::ConnectionContext(
                                          psd->conn_id, transport::ConnHandle{TextWsHandle{ws}},
                                          TransportKind::TextWebSocket));

                                  // Push connect event to event queue
                                  ConnectEvent ev{psd->conn_id};
                                  event_queue_.push(Event{ev});

                                  heartbeat_service_.on_open(psd->conn_id);
                              },
                          .message =
                              [this](UwsSocket* ws, std::string_view data, uWS::OpCode op) {
                                  if (op != uWS::OpCode::TEXT) return;
                                  auto* psd = ws->getUserData();

                                  // Push command request to event queue
                                  CommandRequest req;
                                  req.conn_id = psd->conn_id;
                                  req.payload = std::string{data};
                                  req.received_at = std::chrono::system_clock::now();
                                  event_queue_.push(Event{req});
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

                                  ws->send(status.value(), uWS::OpCode::TEXT);
                              },
                          .close =
                              [this](UwsSocket* ws, int code, std::string_view reason) {
                                  auto* psd = ws->getUserData();
                                  if (!psd) return;

                                  auto conn_id = psd->conn_id;
                                  conns_.detach(conn_id);

                                  // Push disconnect event to event queue
                                  DisconnectEvent ev{conn_id, code, std::string(reason)};
                                  event_queue_.push(Event{ev});
                              },
                      });

    heartbeat_service_.start();
    out_worker_.start();
}

std::string TextWSServer::make_conn_id(void* p) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%p", p);
    return std::string(buf);
}

}  // namespace net::transport::websocket
