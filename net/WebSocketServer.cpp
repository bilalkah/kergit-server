// .cpp
#include "net/WebSocketServer.h"

#include "app/services/HubPublisher.h"

#include <chrono>
#include <nlohmann/json.hpp>
#include <unordered_set>

using nlohmann::json;
using namespace infra::security::validation;
namespace {
std::string channel_topic(const ChannelId& channel_id) { return "channel:" + channel_id.value; }

std::string safe_display(const net::PerSocketData& psd) {
    if (!psd.username.empty()) return psd.username;
    return "Member";
}
}  // namespace
namespace net {

WebSocketServer::WebSocketServer(core::IApp& app, app::Dispatcher& dispatcher,
                                 ConnectionManager& conns, ClientGateway& gateway,
                                 app::services::HubPublisher* hub_publisher,
                                 OriginAllowlist origins, WsLimits limits)
    : app_(app),
      dispatcher_(dispatcher),
      conns_(conns),
      gateway_(gateway),
      hub_publisher_(hub_publisher),
      origins_(std::move(origins)),
      limits_(limits),
      heartbeat_(app, conns) {
    validator_ = std::make_unique<MessageValidator>(dispatcher_);
}

WebSocketServer::~WebSocketServer() { shutdown(); }

void WebSocketServer::wire(const std::string& pattern) {
    // Let dispatcher update PSD on successful auth
    dispatcher_.on_auth_success([this](const ConnId& conn_id, const UserId& user_id) {
        if (hooks_.on_auth) hooks_.on_auth(conn_id, user_id);
    });

    app_.uws().ws<PerSocketData>(
        pattern, {
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

                             json hello = {{"type", "hello"}, {"conn_id", psd->conn_id.value}};
                             gateway_.send_now(psd->conn_id, hello);
                         },
                     .message =
                         [this](UwsSocket* ws, std::string_view data, OpCode op) {
                             if (op != OpCode::TEXT) return;
                             auto* psd = ws->getUserData();

                             auto validation = validator_->validate_message(data);

                             if (!validation.is_valid) {
                                 json error = {{"type", "error"},
                                               {"code", "invalid_message"},
                                               {"message", validation.error_message}};
                                 gateway_.send_now(psd->conn_id, error);
                                 return;
                             }

                             const auto& validated = validation.message;
                             auto type = validated.value("type", std::string{});
                             
                             if (type == "pong") {
                                 heartbeat_.on_pong(*psd);
                                 return;
                             }

                             app::CommandContext ctx{*psd};
                             ctx.input.data = validated;
                             ctx.input.received_at = std::chrono::system_clock::now();

                             dispatcher_.dispatch(type, ctx);

                             if (ctx.output.success) {
                                 gateway_.send_now(psd->conn_id, ctx.output.data);
                             }

                             if (hooks_.on_message_raw) {
                                 hooks_.on_message_raw(psd->conn_id, data);
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
                                 const auto hubs = psd->hub_memberships;
                                 const auto display = safe_display(*psd);
                                 for (const auto& ch : psd->channel_subscriptions) {
                                     json update = {{"type", "presence_update"},
                                                    {"channel_id", ch.value},
                                                    {"handle", display},
                                                    {"display_name", display},
                                                    {"online", false}};
                                     gateway_.publish(channel_topic(ch), update);
                                 }
                                 psd->channel_subscriptions.clear();
                                 gateway_.unsubscribe_all(psd->conn_id);
                                 conns_.detach(psd->conn_id);
                                 if (hub_publisher_ && !hubs.empty()) {
                                     hub_publisher_->publish_hubs(hubs);
                                 }
                                 if (hooks_.on_close) hooks_.on_close(psd->conn_id, code, reason);
                                 psd->authenticated = false;
                                 psd->hub_memberships.clear();
                                 psd->hub_roles.clear();
                             }
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
