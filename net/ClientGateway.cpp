#include "net/ClientGateway.h"

#include <iostream>

namespace net {
using namespace utils;
using json = nlohmann::json;

bool ClientGateway::send_now(const ConnId& conn_id, const json& payload, OpCode op) {
    if (auto* ws = conns_.get(conn_id)) {
        if (debug_)
            log(LogLevel::INFO, "[WS<=] cid=" + conn_id.value + " payload=" + payload.dump());
        return ws->send(payload.dump(), op);
    }
    if (debug_) log(LogLevel::WARN, "[WS<=] cid=" + conn_id.value + " not found");
    return false;
}

void ClientGateway::send_defer(ConnId conn_id, json payload, OpCode op) {
    app_.defer([this, conn = std::move(conn_id), data = std::move(payload), op]() {
        (void)this->send_now(conn, data, op);
    });
}

void ClientGateway::subscribe(const ConnId& cid, const std::string& topic) {
    pubsub_->subscribe(cid, topic);
}
void ClientGateway::unsubscribe(const ConnId& cid, const std::string& topic) {
    pubsub_->unsubscribe(cid, topic);
}
void ClientGateway::unsubscribe_all(const ConnId& cid) { pubsub_->unsubscribe_all(cid); }

void ClientGateway::publish(const std::string& topic, const json& payload, OpCode op) {
    auto subs = pubsub_->subscribers(topic);
    if (subs.empty()) return;
    for (const auto& conn : subs) {
        send_defer(conn, payload, op);
    }
}

}  // namespace net
