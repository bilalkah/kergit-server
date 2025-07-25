#include "ChatClient.h"
#include "ChatClientApp.h"

class ChatClient::Impl {
public:
    Impl(const std::string& server_uri) : app(server_uri) {}
    
    ChatClientApp app;
};

ChatClient::ChatClient(const std::string& server_uri) : pImpl(std::make_unique<Impl>(server_uri)) {}

ChatClient::~ChatClient() = default;

bool ChatClient::connect() {
    return pImpl->app.connect();
}

void ChatClient::disconnect() {
    pImpl->app.disconnect();
}

void ChatClient::reset() {
    pImpl->app.reset();
}

bool ChatClient::is_connected() const {
    return pImpl->app.is_connected();
}

bool ChatClient::join_channel(const std::string& channel, const std::string& username) {
    return pImpl->app.join_channel(channel, username);
}

bool ChatClient::leave_channel() {
    return pImpl->app.leave_channel();
}

bool ChatClient::send_message(const std::string& text) {
    return pImpl->app.send_message(text);
}

bool ChatClient::list_channels() {
    return pImpl->app.list_channels();
}

bool ChatClient::list_users() {
    return pImpl->app.list_users();
}

bool ChatClient::ping() {
    return pImpl->app.ping();
}

void ChatClient::set_message_handler(std::function<void(const json&)> handler) {
    pImpl->app.set_message_handler(handler);
}

void ChatClient::set_connection_handler(std::function<void(bool)> handler) {
    pImpl->app.set_connection_handler(handler);
}

std::string ChatClient::get_username() const {
    return pImpl->app.get_username();
}

std::string ChatClient::get_current_channel() const {
    return pImpl->app.get_current_channel();
}

std::vector<std::string> ChatClient::get_last_channels() const {
    return pImpl->app.get_last_channels();
}

bool ChatClient::wait_for_message(const std::string& type, int timeout_ms) {
    return pImpl->app.wait_for_message(type, timeout_ms);
}

bool ChatClient::wait_for_connection(int timeout_ms) {
    return pImpl->app.wait_for_connection(timeout_ms);
} 