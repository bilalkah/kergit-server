#include "app/AppStack.h"

#include "utils/EnvLoader.h"

#include <chrono>
#include <iostream>

namespace app {

AppStack::AppStack(const core::ServerConfig& config) : config_(config) {
    event_queue_ = std::make_unique<queue::EventQueue>(config_.app_stack.event_queue_capacity);
}

void AppStack::start() { worker_pool_->start(); }

void AppStack::stop() {
    if (voice_service_) {
        voice_service_->stop_reconcile_loop();
    }

    // Stop webhook server
    webhook_server_.stop();

    if (message_service_) {
        message_service_->stopAsyncWriter();
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
    init_redis();
    init_managers();
    init_services();
    voice_service_->recover_from_restart();
    voice_service_->start_reconcile_loop();
    webhook_server_.start();
    init_dispatcher();
    init_workers();
}

app::queue::IEventSink& AppStack::event_sink() { return *event_queue_; }

void AppStack::attach_outbound_sink(net::outbound::IOutboundSink& sink) { out_queue_ = &sink; }

std::uint64_t AppStack::online_user_count() const {
    if (!presence_manager_) return 0;
    return static_cast<std::uint64_t>(presence_manager_->onlineUsers().size());
}

std::uint64_t AppStack::active_webrtc_user_count() const {
    if (!voice_service_) return 0;
    return voice_service_->active_voice_user_count();
}

void AppStack::init_database() {
    try {
        persistence_gateway_ = std::make_unique<PersistenceGateway>(config_.database);
    } catch (const std::exception& e) {
        log(utils::LogLevel::ERROR, "init_database failed: ", e.what());
        throw;
    }
}

void AppStack::init_redis() {
    redis_client_ =
        std::make_unique<infra::redis::RedisClient>(config_.redis.host, config_.redis.port);
}

void AppStack::init_managers() {
    subscription_manager_ = std::make_unique<SubscriptionManager>();
    session_manager_ = std::make_unique<SessionManager>(config_.app_stack.max_sessions_per_user);
}

void AppStack::init_services() {
    try {
        auth_service_ =
            std::make_unique<services::AuthService>(config_.public_endpoints.supabase_issuer());
        presence_manager_ =
            std::make_unique<services::PresenceService>(*session_manager_, *subscription_manager_);
        user_service_ = std::make_unique<services::UserService>(persistence_gateway_->users());
        hub_service_ = std::make_unique<services::HubService>(persistence_gateway_->hubs());
        message_service_ =
            std::make_unique<services::MessageService>(persistence_gateway_->messages());
        if (config_.app_stack.db_write_queue_capacity > 0) {
            message_service_->startAsyncWriter(
                config_.app_stack.db_write_queue_capacity, config_.app_stack.db_write_max_retries,
                std::chrono::milliseconds(config_.app_stack.db_write_retry_ms));
        }

        auto livekit_key = utils::EnvLoader::get_env("LIVEKIT_API_KEY", "");
        auto livekit_secret = utils::EnvLoader::get_env("LIVEKIT_API_SECRET", "");

        voice_service_ = std::make_unique<services::voice::VoiceService>(
            livekit_key, livekit_secret, *redis_client_, *session_manager_, *subscription_manager_,
            *hub_service_, *out_queue_, config_.livekit_cluster.nodes());

        webhook_server_.set_signing_credentials(livekit_key, livekit_secret);

        webhook_server_.set_callback([this](const livekit::webhook::LiveKitEvent& event) {
            voice_service_->on_livekit_event(event);
        });

        invite_service_ = std::make_unique<services::InviteService>(
            *redis_client_, config_.public_endpoints.invite_base_url());

        cmd_ctx_ = std::make_unique<CommandContext>(
            CommandContext{*auth_service_, *message_service_, *hub_service_, *voice_service_,
                           *user_service_, *presence_manager_, *subscription_manager_,
                           *session_manager_, *event_queue_, *invite_service_});

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
