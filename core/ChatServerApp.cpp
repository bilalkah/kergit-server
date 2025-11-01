#include "server/core/ChatServerApp.h"

#include <csignal>
#include <iostream>

// uSockets close symbol is in the uWS headers you already include via Types.h
// Alias is `ListenToken = us_listen_socket_t*;` from Types.h

namespace {

// simple signal latch to trigger stop()
std::atomic<bool> g_stop_requested{false};

void set_signal_handlers() {
    static bool installed = false;
    if (installed) return;
    installed = true;

    auto handler = [](int) { g_stop_requested.store(true); };
    std::signal(SIGINT, handler);
    std::signal(SIGTERM, handler);
#ifdef SIGQUIT
    std::signal(SIGQUIT, handler);
#endif
}

}  // namespace

ChatServerApp::ChatServerApp(ServerConfig cfg) : cfg_(std::move(cfg)) {
    // Build TLS/plain app here so wire_components() can use it
    app_ = AppFactory::create(cfg_);
}

bool ChatServerApp::wire_components() {
    // 1) Register commands
    app::register_all(dispatcher_);

    // 2) Construct WS server and wire routes
    ws_server_ = std::make_unique<net::WebSocketServer>(
        *app_, dispatcher_, connections_, net::OriginAllowlist{},  // customize allowlist later
        net::WsLimits{256 * 1024}                                  // 256 KB max message
    );

    // Optional: hooks for logging / side-effects on lifecycle
    ws_server_->set_hooks(net::WsHooks{
        .on_open =
            [&](const std::string& cid) { std::cerr << "[WS] open conn_id=" << cid << "\n"; },
        .on_close =
            [&](const std::string& cid, int code, std::string_view reason) {
                std::cerr << "[WS] close conn_id=" << cid << " code=" << code
                          << " reason=" << std::string(reason) << "\n";
            },
        .on_message_raw =
            [&](const std::string& cid, std::string_view raw) {
                // lightweight trace; make it debug if noisy
                // std::cerr << "[WS] recv cid=" << cid << " " << raw << "\n";
            },
        .on_auth =
            [&](const std::string& cid, const std::string& uid) {
                std::cerr << "[WS] auth conn_id=" << cid << " user_id=" << uid << "\n";
            }});

    // 3) Register ws endpoint
    ws_server_->wire("/ws");

    // 4) Listen and keep listen token so we can stop gracefully
    bool ok = true;
    listen_token_ = nullptr;
    app_->uws().listen(cfg_.port, [&](auto* token) {
        listen_token_ = token;
        ok = (token != nullptr);
        if (ok) {
            std::cerr << "[SERVER] Listening on " << (cfg_.tls_enabled ? "wss" : "ws") << "://"
                      << cfg_.host << ":" << cfg_.port << " (pattern /ws)\n";
        }
    });

    if (!ok) {
        std::cerr << "[ERROR] Failed to bind port " << cfg_.port << "\n";
        return false;
    }

    return true;
}

bool ChatServerApp::start_blocking() {
    if (running_.exchange(true)) return false;  // already running
    set_signal_handlers();

    if (!wire_components()) {
        running_.store(false);
        return false;
    }

    // Run until stop() closes listen socket
    std::cerr << "[SERVER] Starting event loop (blocking)...\n";
    app_->uws().run();  // blocks

    running_.store(false);
    std::cerr << "[SERVER] Event loop exited.\n";
    return true;
}

bool ChatServerApp::start_async() {
    if (running_.exchange(true)) return false;  // already running
    set_signal_handlers();

    if (!wire_components()) {
        running_.store(false);
        return false;
    }

    std::cerr << "[SERVER] Starting event loop (async)...\n";
    loop_thread_ = std::thread([this] {
        app_->uws().run();  // blocks in this thread
        running_.store(false);
        std::cerr << "[SERVER] Event loop exited (async thread).\n";
    });

    // Optionally, a tiny supervisor that watches the signal latch in a background thread
    std::thread([this] {
        while (running_.load()) {
            if (g_stop_requested.load()) {
                this->stop();
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }).detach();

    return true;
}

void ChatServerApp::stop() {
    if (!running_.load()) return;

    // Close the listen socket: uWS will stop accepting new conns,
    // and the loop will exit once existing handlers drain.
    if (listen_token_) {
        us_listen_socket_close(listen_token_);
        listen_token_ = nullptr;
    }
    // If you also keep a uWS::Loop* you can post a deferred "graceful shutdown" task here
    // to close all active connections from ConnectionManager, etc.
}

void ChatServerApp::join() {
    if (loop_thread_.joinable()) {
        loop_thread_.join();
    }
}
