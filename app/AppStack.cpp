#include "app/AppStack.h"

namespace app {

AppStack::AppStack(const core::ServerConfig& config) : config_(config) {
    event_queue_ = std::make_unique<queue::EventQueue>();
}

void AppStack::start() { worker_pool_->start(); }

void AppStack::stop() { worker_pool_->stop(); }

void AppStack::pause() { worker_pool_->pause(); }

void AppStack::resume() { worker_pool_->resume(); }

void AppStack::init_database() {
    persistence_gateway_ = std::make_unique<PersistenceGateway>(
        config_.database.to_connection_string(), config_.database.pool_size);
}

void AppStack::init_managers() {
    subscription_manager_ = std::make_unique<SubscriptionManager>();
    session_manager_ = std::make_unique<SessionManager>();
}

void AppStack::init_services() {
    auth_service_ = std::make_unique<services::AuthService>();
    public_id_service_ = std::make_unique<services::PublicIdService>();
    presence_manager_ =
        std::make_unique<services::PresenceService>(*session_manager_, *subscription_manager_);
    user_service_ = std::make_unique<services::UserService>(persistence_gateway_->users());
    channel_service_ = std::make_unique<services::ChannelService>(persistence_gateway_->channels());
    hub_service_ = std::make_unique<services::HubService>(persistence_gateway_->hubs());
    hub_notifier_ = std::make_unique<services::HubNotifier>(*public_id_service_);
    hub_snapshot_builder_ = std::make_unique<services::HubSnapshotBuilder>(
        *channel_service_, *hub_service_, *presence_manager_, *public_id_service_);

    cmd_ctx_ = std::make_unique<CommandContext>(CommandContext{
        *auth_service_, *channel_service_, *hub_service_, *hub_notifier_, *hub_snapshot_builder_,
        *user_service_, *presence_manager_, *subscription_manager_, *session_manager_});
}

void AppStack::init_dispatcher() {
    dispatcher_ = std::make_unique<Dispatcher>();
    dispatcher_->register_all();
}

void AppStack::init_workers() {
    worker_pool_ = std::make_unique<worker::WorkerPool>(*event_queue_, *out_queue_, *dispatcher_,
                                                        *cmd_ctx_, config_.app_stack);
}

}  // namespace app
