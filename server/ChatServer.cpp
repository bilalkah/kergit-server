#include "ChatServer.h"
#include "ChatServerApp.h"

class ChatServer::Impl {
public:
    Impl(int port) : app(port) {}
    
    ChatServerApp app;
};

ChatServer::ChatServer(int port) : pImpl(std::make_unique<Impl>(port)) {}

ChatServer::~ChatServer() {
    if (pImpl) {
        pImpl->app.stop();
    }
}

bool ChatServer::start() {
    return pImpl->app.start();
}

void ChatServer::stop() {
    pImpl->app.stop();
}

bool ChatServer::is_running() const {
    return pImpl->app.is_running();
}

int ChatServer::get_port() const {
    return pImpl->app.get_port();
}

void ChatServer::set_connection_handler(std::function<void(const std::string&)> handler) {
    pImpl->app.set_connection_handler(handler);
}

void ChatServer::set_disconnection_handler(std::function<void(const std::string&)> handler) {
    pImpl->app.set_disconnection_handler(handler);
} 