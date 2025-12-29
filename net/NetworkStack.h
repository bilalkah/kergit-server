#ifndef NET_NETWORKSTACK_H
#define NET_NETWORKSTACK_H

#include "app/queue/EventQueue.h"
#include "core/ServerConfig.h"
#include "net/connection/ConnectionRegistery.h"
#include "net/outbound/OutgoingQueue.h"
#include "net/transport/ITransport.h"
#include "net/transport/websocket/TextWebSocketTransport.h"
#include "utils/Loggable.h"

#include <cstdint>
#include <memory>
#include <string>
#include <thread>

namespace net {

using LoopId = std::uintptr_t;

class NetworkStack : utils::Loggable {
   public:
    NetworkStack(EventQueue& event_queue, core::NetworkStackConfig cfg_ = {});
    ~NetworkStack();

    // Identity of the loop this stack lives on
    LoopId loop_id() const;

    // Start/stop lifecycle
    std::expected<bool, std::string> start();
    std::expected<bool, std::string> stop();

   private:
    /**
     * Configuration
     */
    core::NetworkStackConfig cfg_{};

    /**
     * References
     */
    EventQueue& event_queue_;

    /**
     * Server thread
     */
    std::jthread server_thread_;
    std::atomic<bool> started_{false};
    std::atomic<bool> running_{false};
    std::atomic<bool> stopped_{false};
    void run_server();
    void wire_components();

    /**
     * Transport server instance
     */
    std::unique_ptr<transport::ITransportServer> transport_layer_;

    /**
     * Connection registry
     */
    std::unique_ptr<connection::ConnectionRegistery> connection_registry_;

    /**
     * Outgoing message queue
     */
    std::unique_ptr<outbound::OutgoingQueue> outgoing_queue_;
};

}  // namespace net

#endif  // NET_NETWORKSTACK_H
