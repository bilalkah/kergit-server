#ifndef APP_APPSTACK_H
#define APP_APPSTACK_H

#include "app/dispatcher/Dispatcher.h"
#include "app/worker/WorkerPool.h"
#include "core/ServerConfig.h"
#include "utils/Loggable.h"

// include services headers
#include "app/services/PublicIdService.h"
#include "app/services/auth/AuthService.h"
#include "app/services/channel/ChannelService.h"
#include "app/services/hub/HubNotifier.h"
#include "app/services/hub/HubService.h"
#include "app/services/hub/SnapshotBuilder.h"
#include "app/services/presence/PresenceService.h"
#include "app/services/user/UserService.h"

// include managers headers
#include "app/managers/session/SessionManager.h"
#include "app/managers/subscription/SubscriptionManager.h"

// include repositories headers
#include "infra/persistence/PersistenceGateway.h"

namespace app {

class AppStack : public utils::Loggable {
   public:
    explicit AppStack(const core::ServerConfig& config);

    void start();
    void stop();
    void pause();
    void resume();

    void bootstrap() {
        if (!out_queue_) {
            throw std::runtime_error("Out queue not initialized before bootstrap");
        }
        init_database();
        init_managers();
        init_services();
        init_dispatcher();
        init_workers();
    }

    app::queue::IEventSink& EventSink() { return *event_queue_; }
    void AttachOutboundSink(net::outbound::IOutboundSink& sink) { out_queue_ = &sink; }

   private:
    void init_database();
    void init_managers();
    void init_services();
    void init_dispatcher();
    void init_workers();

   private:
    core::ServerConfig config_;
    net::outbound::IOutboundSink* out_queue_{nullptr};

    // database
    std::unique_ptr<PersistenceGateway> persistence_gateway_;

    // Managers
    std::unique_ptr<SubscriptionManager> subscription_manager_;
    std::unique_ptr<SessionManager> session_manager_;

    // Services
    std::unique_ptr<services::AuthService> auth_service_;
    std::unique_ptr<services::PublicIdService> public_id_service_;
    std::unique_ptr<services::PresenceService> presence_manager_;
    std::unique_ptr<services::UserService> user_service_;
    std::unique_ptr<services::ChannelService> channel_service_;
    std::unique_ptr<services::HubService> hub_service_;
    std::unique_ptr<services::HubNotifier> hub_notifier_;
    std::unique_ptr<services::HubSnapshotBuilder> hub_snapshot_builder_;

    // Core
    std::unique_ptr<CommandContext> cmd_ctx_;
    std::unique_ptr<queue::EventQueue> event_queue_;
    std::unique_ptr<Dispatcher> dispatcher_;
    std::unique_ptr<worker::WorkerPool> worker_pool_;
};

}  // namespace app

#endif  // APP_APPSTACK_H
