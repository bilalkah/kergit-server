#ifndef NET_NETWORKSTACK_H
#define NET_NETWORKSTACK_H

#include "app/queue/IEventSink.h"
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
    NetworkStack(core::NetworkStackConfig cfg_ = {});
    ~NetworkStack();

    // Identity of the loop this stack lives on
    LoopId loop_id() const;

    // Start/stop lifecycle
    bool start();
    bool stop();

    net::outbound::IOutboundSink& outbound_sink();

    /**
     * Attach event sink to send events to App layer
     * @param sink Event sink reference
     * Without this, the stack cannot start
     */
    void attach_event_sink(app::queue::IEventSink& sink);
    NetStackId id() const;

   private:
    /**
     * NetstackId
     */
    NetStackId id_;
    /**
     * Configuration
     */
    core::NetworkStackConfig cfg_{};

    /**
     * References
     */
    app::queue::IEventSink* event_sink_{nullptr};

    /**
     * Server thread
     */
    std::jthread server_thread_;
    std::atomic<bool> started_{false};
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

    class IdGenerator {
       public:
        static NetStackId next(const std::string& prefix = "ns") {
            static std::atomic<uint64_t> counter{1};
            return NetStackId(prefix + "-" +
                              std::to_string(counter.fetch_add(1, std::memory_order_relaxed)));
        }
    };
};

}  // namespace net

#endif  // NET_NETWORKSTACK_H
