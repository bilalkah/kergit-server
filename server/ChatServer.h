#pragma once

#include <functional>
#include <memory>
#include <string>

class ChatServer {
   public:
    ChatServer(int port = 9001);
    ~ChatServer();

    // Server control
    bool start();
    void stop();
    bool is_running() const;
    int get_port() const;

    // Event handlers
    void set_connection_handler(std::function<void(const std::string&)> handler);
    void set_disconnection_handler(std::function<void(const std::string&)> handler);

   private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};