#include "core/ChatServerApp.h"

#include <iostream>

namespace core {

using namespace utils;

ChatServerApp::ChatServerApp(ServerConfig& cfg) {
    cfg_ = std::move(cfg);
    persistence_gateway_ptr_ = std::make_unique<PersistenceGateway>(
        cfg_.database.to_connection_string(), cfg_.database.pool_size);
    out_queue_ptr_ = std::make_unique<OutgoingQueue>();
    in_queue_ptr_ = std::make_unique<EventQueue>();
    cache_ptr_ = std::make_unique<app::memory::OnMemoryCache>();
}

ChatServerApp::~ChatServerApp() {}

bool ChatServerApp::wire_components() {
    app::register_all(*dispatcher_ptr_, *service_objects_ptr_);

    ws_server_ = std::make_unique<net::WebSocketServer>(*app_ptr_, *connections_ptr_, *gateway_ptr_,
                                                        *in_queue_ptr_, *out_queue_ptr_,
                                                        net::OriginAllowlist{}, net::WsLimits{});

    // Optional: hooks for logging / side-effects on lifecycle
    ws_server_->set_hooks(net::WsHooks{
        .on_open = [&](const ConnId& cid) { log(LogLevel::INFO, "open conn_id:" + cid.value); },
        .on_close =
            [&](const ConnId& cid, int code, std::string_view reason) {
                log(LogLevel::INFO, "close conn_id:" + cid.value + " code:" + std::to_string(code) +
                                        " reason:" + std::string(reason));
            },
        .on_message_raw =
            [&](const ConnId& cid, std::string_view raw) {
                log(LogLevel::INFO, "message conn_id:" + cid.value + " raw:" + std::string(raw));
            },
        .on_auth =
            [&](const ConnId& cid, const UserId& uid) {
                // log(LogLevel::INFO, "auth success conn_id:" + cid.value + " user_id:" +
                // uid.value);
            }});

    // 3) Register ws endpoint
    ws_server_->wire(cfg_.ws_path);
    if (hub_publisher_) hub_publisher_->start();

    return true;
}

bool ChatServerApp::start() {
    if (running_.exchange(true) || started_.exchange(true))
        return false;  // already running or started

    log(LogLevel::WARN, "Starting event loop thread...");
    loop_thread_ = std::thread(&ChatServerApp::run_server, this);

    auto timeout = std::chrono::seconds(5);
    auto start_time = std::chrono::system_clock::now();
    while (!started_.load()) {
        if (std::chrono::system_clock::now() - start_time > timeout) {
            log(LogLevel::ERROR, "Failed to start event loop within timeout.");
            running_.store(false);
            started_.store(false);
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!running_.load()) {
        log(LogLevel::ERROR, "Event loop failed to start.");
        started_.store(false);
        return false;
    }
    started_.store(true);
    return true;
}

void ChatServerApp::run_server() {
    log(LogLevel::WARN, "Starting run_server");
    app_ptr_ = AppFactory::create(cfg_);
    dispatcher_ptr_ = std::make_unique<app::Dispatcher>();
    connections_ptr_ = std::make_unique<net::ConnectionManager>();
    gateway_ptr_ =
        std::make_unique<net::ClientGateway>(*app_ptr_, *connections_ptr_, cfg_.debug_gateway);
    hub_publisher_ = std::make_unique<app::services::HubPublisher>(
        *app_ptr_, *persistence_gateway_ptr_, *connections_ptr_, *out_queue_ptr_,
        persistence_gateway_ptr_->ids());
    worker_pool_ptr_ =
        std::make_unique<app::WorkerPool>(*in_queue_ptr_, *out_queue_ptr_, *dispatcher_ptr_);
    worker_pool_ptr_->start();
    service_objects_ptr_ = std::make_unique<app::ServiceObjects>(
        *persistence_gateway_ptr_, *gateway_ptr_, *connections_ptr_, *hub_publisher_,
        persistence_gateway_ptr_->ids(), *cache_ptr_);
    if (!wire_components()) {
        running_.store(false);
        return;
    }

    listen_token_ = nullptr;
    app_ptr_->uws().listen(cfg_.host, cfg_.port, [&](auto* token) {
        if (token) {
            log(LogLevel::INFO, "Listener bound successfully.");
            running_.store(true);
            listen_token_ = token;
        } else {
            running_.store(false);
        }
    });

    app_ptr_->uws().run();

    log(LogLevel::WARN, "Exiting run_server");
    if (hub_publisher_) hub_publisher_->stop();
    app_ptr_.reset();
    listen_token_ = nullptr;
    running_.store(false);
}

void ChatServerApp::stop() {
    log(LogLevel::WARN, "Stop ChatServerApp is requested.");

    // Guarantee we only stop once
    bool was_running = running_.exchange(false);
    if (!was_running) {
        log(LogLevel::INFO, "ChatServerApp already stopping / stopped.");
        return;
    }

    if (hub_publisher_) hub_publisher_->stop();
    if (ws_server_) ws_server_->shutdown();
    if (worker_pool_ptr_) worker_pool_ptr_->stop();

    // Ask the loop thread to close the app
    if (app_ptr_) {
        auto* loop = app_ptr_->uws().getLoop();
        loop->defer([this]() {
            log(LogLevel::INFO, "Defer close uWS app loop from stop()");
            app_ptr_->uws().close();
        });
    }

    // Now wait for run_server() to finish
    join();
    stopped_.store(true);
    log(LogLevel::INFO, "Stop WebSocket is completed.");
}

void ChatServerApp::join() {
    if (loop_thread_.joinable()) {
        log(LogLevel::INFO, "Joining event loop thread...");
        loop_thread_.join();
    } else {
        log(LogLevel::INFO, "Event loop thread not joinable.");
    }
}

}  // namespace core
