#ifndef CORE_CHATSERVERAPP_H
#define CORE_CHATSERVERAPP_H

#include "app/CommandRegistry.h"
#include "app/Dispatcher.h"
#include "core/AppFactory.h"
#include "core/IApp.h"
#include "core/ServerConfig.h"
#include "core/Types.h"
#include "net/ConnectionManager.h"
#include "net/WebSocketServer.h"

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>

class ChatServerApp {
   public:
    explicit ChatServerApp(ServerConfig cfg);

    // Blocking run (calls uWS::App::run and never returns until stop)
    bool start_blocking();

    // Non-blocking: starts the loop in a worker thread
    bool start_async();

    // Request graceful stop (closes the listen socket)
    void stop();

    // Join the worker thread if started async
    void join();

    bool is_running() const { return running_.load(); }

   private:
    bool wire_components();  // build dispatcher, ws server, hooks, listen

    // --- Composition state
    ServerConfig cfg_;
    std::unique_ptr<IApp> app_;
    app::Dispatcher dispatcher_;
    net::ConnectionManager connections_;
    std::unique_ptr<net::WebSocketServer> ws_server_;

    // listen socket token from uSockets (needed to close)
    using ListenToken = ::us_listen_socket_t*;
    ListenToken listen_token_{nullptr};

    // lifecycle
    std::atomic<bool> running_{false};
    std::thread loop_thread_;
};

#endif  // CORE_CHATSERVERAPP_H
