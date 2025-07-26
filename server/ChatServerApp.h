#pragma once

#include "App.h"
#include "common/ChatServer.h"
#include "server/commands/AllCommands.h"

#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <unordered_map>

struct PerSocketData {
    std::string user_id;
};

class ChatServerApp {
   public:
    ChatServerApp(int port = 9001);
    ~ChatServerApp();

    // Server control
    bool start();
    void stop();
    bool is_running() const { return running; }
    int get_port() const { return port; }

    // Access to internal state (for testing)
    ChatServerState& get_chat_server() { return chatServer; }
    const std::unordered_map<uWS::WebSocket<false, true, PerSocketData>*, std::string>&
    get_connections() const {
        return ws_to_user;
    }

    // Event handlers
    void set_connection_handler(std::function<void(const std::string&)> handler);
    void set_disconnection_handler(std::function<void(const std::string&)> handler);

   private:
    void setup_commands();
    void run_server();

    int port;
    std::atomic<bool> running{false};
    std::thread server_thread;

    ChatServerState chatServer;
    std::unordered_map<uWS::WebSocket<false, true, PerSocketData>*, std::string> ws_to_user;
    std::unordered_map<std::string, std::unique_ptr<ICommand>> command_map;

    std::function<void(const std::string&)> connection_handler;
    std::function<void(const std::string&)> disconnection_handler;

    // uWS server instance for proper shutdown
    std::unique_ptr<uWS::App> app;
};