#ifndef NET_CLIENTGATEWAY_H
#define NET_CLIENTGATEWAY_H

#include "app/pubsub/PubSub.h"
#include "core/IApp.h"
#include "core/Types.h"
#include "domains/ids/Ids.h"
#include "net/ConnectionManager.h"
#include "utils/Loggable.h"

#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

namespace net {

class ClientGateway : public utils::Loggable {
   public:
    ClientGateway(core::IApp& app, ConnectionManager& conns, bool debug = false)
        : app_(app), conns_(conns), debug_(debug) {
        pubsub_ = std::make_unique<app::PubSub>();
    }

    // same-thread send
    bool send_now(const ConnId& conn_id, const nlohmann::json& payload, OpCode op = OpCode::TEXT);

    // cross-thread safe
    void send_defer(ConnId conn_id, nlohmann::json payload, OpCode op = OpCode::TEXT);

    // pub/sub helpers
    void subscribe(const ConnId& cid, const std::string& topic);
    void unsubscribe(const ConnId& cid, const std::string& topic);
    void unsubscribe_all(const ConnId& cid);
    void publish(const std::string& topic, const nlohmann::json& payload, OpCode op = OpCode::TEXT);
    void publish(const std::string& topic, const std::string& payload, OpCode op = OpCode::TEXT);

    // Say hi
    void say_hello(const ConnId& cid) {
        nlohmann::json hello = {{"type", "hello"}, {"conn_id", cid.value}};
        send_defer(cid, hello);
    }

   private:
    ::core::IApp& app_;
    ConnectionManager& conns_;
    std::unique_ptr<app::PubSub> pubsub_;
    bool debug_;
};

}  // namespace net

#endif  // NET_CLIENTGATEWAY_H
