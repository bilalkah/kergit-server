#ifndef APP_APPSTACK_H
#define APP_APPSTACK_H

#include "app/dispatcher/CommandContext.h"
#include "app/dispatcher/Dispatcher.h"
#include "app/services/invite/InviteService.h"
#include "app/services/voice/VoiceService.h"
#include "infra/redis/RedisClient.h"
#include "app/worker/WorkerPool.h"
#include "core/ServerConfig.h"
#include "infra/persistence/PersistenceGateway.h"
#include "utils/Loggable.h"
#include "livekit/webhook/LivekitWebhookServer.h"

namespace app {

class AppStack : public utils::Loggable {
   public:
    explicit AppStack(const core::ServerConfig& config);

    void start();
    void stop();
    void pause();
    void resume();

    void bootstrap();

    app::queue::IEventSink& event_sink();
    void attach_outbound_sink(net::outbound::IOutboundSink& sink);

   private:
    void init_database();
    void init_redis();
    void init_managers();
    void init_services();
    void init_dispatcher();
    void init_workers();

   private:
    core::ServerConfig config_;
    net::outbound::IOutboundSink* out_queue_{nullptr};

    // database
    std::unique_ptr<PersistenceGateway> persistence_gateway_;

    // Redis
    std::unique_ptr<infra::redis::RedisClient> redis_client_;

    // Managers
    std::unique_ptr<SubscriptionManager> subscription_manager_;
    std::unique_ptr<SessionManager> session_manager_;

    // Services
    std::unique_ptr<services::AuthService> auth_service_;
    std::unique_ptr<services::PresenceService> presence_manager_;
    std::unique_ptr<services::UserService> user_service_;
    std::unique_ptr<services::ChannelService> channel_service_;
    std::unique_ptr<services::HubService> hub_service_;
    std::unique_ptr<services::HubNotifier> hub_notifier_;
    std::unique_ptr<services::HubSnapshotBuilder> hub_snapshot_builder_;
    std::unique_ptr<services::voice::VoiceService> voice_service_;
    std::unique_ptr<services::InviteService> invite_service_;

    // Core
    std::unique_ptr<CommandContext> cmd_ctx_;
    std::unique_ptr<queue::EventQueue> event_queue_;
    std::unique_ptr<Dispatcher> dispatcher_;
    std::unique_ptr<worker::WorkerPool> worker_pool_;

    // Webhook server for LiveKit events
    livekit::webhook::LivekitWebhookServer webhook_server_;
};

}  // namespace app

#endif  // APP_APPSTACK_H
