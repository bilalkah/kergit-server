#include "app/AppStack.h"

#include "utils/EnvLoader.h"

#include <chrono>

namespace app {

AppStack::AppStack(const core::ServerConfig& config) : config_(config) {
    event_queue_ = std::make_unique<queue::EventQueue>(config_.app_stack.event_queue_capacity);
}

void AppStack::start() { worker_pool_->start(); }

void AppStack::stop() {
    if (channel_service_) {
        channel_service_->stopAsyncWriter();
    }
    worker_pool_->stop();
}

void AppStack::pause() { worker_pool_->pause(); }

void AppStack::resume() { worker_pool_->resume(); }

void AppStack::bootstrap() {
    if (!out_queue_) {
        throw std::runtime_error("Outbound sink not attached to AppStack");
    }
    init_database();
    init_managers();
    init_services();
    init_dispatcher();
    init_workers();
}

app::queue::IEventSink& AppStack::event_sink() { return *event_queue_; }

void AppStack::attach_outbound_sink(net::outbound::IOutboundSink& sink) { out_queue_ = &sink; }

void AppStack::init_database() {
    persistence_gateway_ = std::make_unique<PersistenceGateway>(
        config_.database.to_connection_string(), config_.database.read_pool_size,
        config_.database.write_pool_size);
}

void AppStack::init_managers() {
    subscription_manager_ = std::make_unique<SubscriptionManager>();
    session_manager_ = std::make_unique<SessionManager>();
}

void AppStack::init_services() {
    try {
        auth_service_ = std::make_unique<services::AuthService>();
        public_id_service_ = std::make_unique<services::PublicIdService>();
        presence_manager_ =
            std::make_unique<services::PresenceService>(*session_manager_, *subscription_manager_);
        user_service_ = std::make_unique<services::UserService>(persistence_gateway_->users());
        channel_service_ =
            std::make_unique<services::ChannelService>(persistence_gateway_->channels());
        if (config_.app_stack.db_write_queue_capacity > 0) {
            channel_service_->startAsyncWriter(
                config_.app_stack.db_write_queue_capacity, config_.app_stack.db_write_max_retries,
                std::chrono::milliseconds(config_.app_stack.db_write_retry_ms));
        }
        hub_service_ = std::make_unique<services::HubService>(persistence_gateway_->hubs(),
                                                              persistence_gateway_->channels());
        channel_service_->setHubService(*hub_service_);
        hub_notifier_ = std::make_unique<services::HubNotifier>(*public_id_service_);
        hub_snapshot_builder_ = std::make_unique<services::HubSnapshotBuilder>(
            *channel_service_, *hub_service_, *presence_manager_, *public_id_service_);

        auto livekit_key = utils::EnvLoader::get_env("LIVEKIT_API_KEY", "");
        auto livekit_secret = utils::EnvLoader::get_env("LIVEKIT_API_SECRET", "");

        livekit_token_service_ =
            std::make_unique<services::livekit::LiveKitTokenService>(livekit_key, livekit_secret);
        cmd_ctx_ = std::make_unique<CommandContext>(CommandContext{
            *public_id_service_, *auth_service_, *channel_service_, *hub_service_, *hub_notifier_,
            *hub_snapshot_builder_, *livekit_token_service_, *user_service_, *presence_manager_,
            *subscription_manager_, *session_manager_, *event_queue_});

    } catch (const std::exception& ex) {
        log(utils::LogLevel::ERROR, "init_services failed: ", ex.what());
        throw;
    }
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
