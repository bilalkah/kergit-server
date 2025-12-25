#ifndef CORE_CHATSERVERAPP_H
#define CORE_CHATSERVERAPP_H

#include "app/CommandRegistery.h"
#include "app/Dispatcher.h"
#include "app/queue/EventQueue.h"
#include "app/queue/OutgoingQueue.h"
#include "app/services/HubPublisher.h"
#include "app/memory/OnMemoryCache.h"
#include "app/worker/WorkerPool.h"
#include "core/AppFactory.h"
#include "core/IApp.h"
#include "core/ServerConfig.h"
#include "core/Types.h"
#include "infra/persistence/PersistenceGateway.h"
#include "net/ClientGateway.h"
#include "net/ConnectionManager.h"
#include "net/WebSocketServer.h"
#include "utils/Loggable.h"

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>

namespace core {

class ChatServerApp : public utils::Loggable {
   public:
    explicit ChatServerApp(ServerConfig& cfg);
    ~ChatServerApp();
    bool start();
    void stop();

    bool is_started() const { return started_.load(); }
    bool is_running() const { return running_.load(); }
    bool is_stopped() const { return stopped_.load(); }

   private:
    bool wire_components();
    void run_server();
    void join();

    ServerConfig cfg_{};
    std::unique_ptr<IApp> app_ptr_;
    std::unique_ptr<app::Dispatcher> dispatcher_ptr_;
    std::unique_ptr<net::ConnectionManager> connections_ptr_;
    std::unique_ptr<net::ClientGateway> gateway_ptr_;
    std::unique_ptr<net::WebSocketServer> ws_server_;
    std::unique_ptr<PersistenceGateway> persistence_gateway_ptr_;
    std::unique_ptr<app::services::HubPublisher> hub_publisher_;
    std::unique_ptr<EventQueue> in_queue_ptr_;
    std::unique_ptr<OutgoingQueue> out_queue_ptr_;
    std::unique_ptr<app::WorkerPool> worker_pool_ptr_;
    std::unique_ptr<app::memory::ICache> cache_ptr_;
    std::unique_ptr<app::ServiceObjects> service_objects_ptr_;

    ListenToken listen_token_{nullptr};

    std::atomic<bool> started_{false};
    std::atomic<bool> running_{false};
    std::atomic<bool> stopped_{false};
    std::thread loop_thread_;
};

}  // namespace core

#endif  // CORE_CHATSERVERAPP_H
