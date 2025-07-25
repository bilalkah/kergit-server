#pragma once

#include <string>
#include <functional>
#include <nlohmann/json.hpp>
#include <vector>
#include <memory>

using json = nlohmann::json;

class ChatClient {
public:
    ChatClient(const std::string& server_uri = "ws://localhost:9001");
    ~ChatClient();

    // Connection management
    bool connect();
    void disconnect();
    bool is_connected() const;
    void reset();

    // Chat operations
    bool join_channel(const std::string& channel, const std::string& username = "");
    bool leave_channel();
    bool send_message(const std::string& text);
    bool list_channels();
    bool list_users();
    bool ping();

    // Message handling
    void set_message_handler(std::function<void(const json&)> handler);
    void set_connection_handler(std::function<void(bool)> handler);
    
    // Getters
    std::string get_username() const;
    std::string get_current_channel() const;
    std::vector<std::string> get_last_channels() const;

    // Wait utilities (for testing)
    bool wait_for_message(const std::string& type, int timeout_ms = 5000);
    bool wait_for_connection(int timeout_ms = 5000);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
}; 