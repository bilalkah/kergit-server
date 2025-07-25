#include "ChatClientApp.h"
#include <iostream>
#include <chrono>

using namespace std::chrono;

ChatClientApp::ChatClientApp(const std::string& server_uri) : server_uri(server_uri) {
    ws_client.set_access_channels(websocketpp::log::alevel::none);
    ws_client.clear_access_channels(websocketpp::log::alevel::frame_payload);
    ws_client.init_asio();
    
    ws_client.set_message_handler(std::bind(&ChatClientApp::on_message, this, std::placeholders::_1, std::placeholders::_2));
    ws_client.set_open_handler(std::bind(&ChatClientApp::on_open, this, std::placeholders::_1));
    ws_client.set_close_handler(std::bind(&ChatClientApp::on_close, this, std::placeholders::_1));
    ws_client.set_fail_handler(std::bind(&ChatClientApp::on_fail, this, std::placeholders::_1));
}

ChatClientApp::~ChatClientApp() {
    disconnect();
    
    // Ensure the WebSocket client is fully stopped before destruction
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
}

bool ChatClientApp::connect() {
    if (connected) return true;
    
    websocketpp::lib::error_code ec;
    auto conn = ws_client.get_connection(server_uri, ec);
    if (ec) {
        std::cerr << "Could not create connection: " << ec.message() << std::endl;
        return false;
    }
    
    connection = conn->get_handle();
    ws_client.connect(conn);
    
    should_stop = false;
    client_thread = std::thread([this]() {
        try {
            ws_client.run();
        } catch (const std::exception& e) {
            std::cerr << "Client thread exception: " << e.what() << std::endl;
        }
    });
    
    return wait_for_connection();
}

void ChatClientApp::disconnect() {
    if (!connected) return;
    
    should_stop = true;
    connected = false;
    
    try {
        // Close the connection first
        ws_client.close(connection, websocketpp::close::status::normal, "Client disconnect");
        
        // Stop the WebSocket client
        ws_client.stop();
        
        // Wait for thread to finish with a short timeout
        if (client_thread.joinable()) {
            auto start = std::chrono::steady_clock::now();
            while (client_thread.joinable() && 
                   std::chrono::steady_clock::now() - start < std::chrono::milliseconds(500)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            
            if (client_thread.joinable()) {
                // If thread is still running, detach it to avoid hanging
                client_thread.detach();
            }
        }
        
        // Wait a bit more to ensure the WebSocket client is fully stopped
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Clear message handler to prevent any callbacks during cleanup
        message_handler = nullptr;
        connection_handler = nullptr;
    } catch (const std::exception& e) {
        // Ignore exceptions during disconnect
    }
}

void ChatClientApp::reset() {
    disconnect();
    
    // Wait a bit more to ensure the WebSocket client is fully stopped
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Reset all state
    connected = false;
    should_stop = false;
    username.clear();
    current_channel.clear();
    last_channels.clear();
    received_messages.clear();
    message_handler = nullptr;
    connection_handler = nullptr;
    
    // Reinitialize the WebSocket client
    try {
        // Reset the WebSocket client properly
        ws_client.reset();
        ws_client.set_access_channels(websocketpp::log::alevel::none);
        ws_client.clear_access_channels(websocketpp::log::alevel::frame_payload);
        ws_client.init_asio();
        
        ws_client.set_message_handler(std::bind(&ChatClientApp::on_message, this, std::placeholders::_1, std::placeholders::_2));
        ws_client.set_open_handler(std::bind(&ChatClientApp::on_open, this, std::placeholders::_1));
        ws_client.set_close_handler(std::bind(&ChatClientApp::on_close, this, std::placeholders::_1));
        ws_client.set_fail_handler(std::bind(&ChatClientApp::on_fail, this, std::placeholders::_1));
    } catch (const std::exception& e) {
        std::cerr << "Failed to reset WebSocket client: " << e.what() << std::endl;
    }
}

bool ChatClientApp::join_channel(const std::string& channel, const std::string& username) {
    if (!connected) return false;
    
    json msg = {
        {"type", "join"},
        {"channel", channel},
        {"username", username.empty() ? this->username : username}
    };
    
    ws_client.send(connection, msg.dump(), websocketpp::frame::opcode::text);
    current_channel = channel;
    return true;
}

bool ChatClientApp::leave_channel() {
    if (!connected) return false;
    
    json msg = {
        {"type", "join"},
        {"channel", ""},
        {"username", username}
    };
    
    ws_client.send(connection, msg.dump(), websocketpp::frame::opcode::text);
    current_channel.clear();
    return true;
}

bool ChatClientApp::send_message(const std::string& text) {
    if (!connected || current_channel.empty()) return false;
    
    json msg = {
        {"type", "chat"},
        {"text", text}
    };
    
    ws_client.send(connection, msg.dump(), websocketpp::frame::opcode::text);
    return true;
}

bool ChatClientApp::list_channels() {
    if (!connected) return false;
    
    json msg = {{"type", "list"}};
    ws_client.send(connection, msg.dump(), websocketpp::frame::opcode::text);
    return true;
}

bool ChatClientApp::list_users() {
    if (!connected || current_channel.empty()) return false;
    
    json msg = {{"type", "users"}};
    ws_client.send(connection, msg.dump(), websocketpp::frame::opcode::text);
    return true;
}

bool ChatClientApp::ping() {
    if (!connected) return false;
    
    auto now = steady_clock::now();
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()).count();
    
    json msg = {
        {"type", "ping"},
        {"timestamp", ms}
    };
    
    ws_client.send(connection, msg.dump(), websocketpp::frame::opcode::text);
    return true;
}

void ChatClientApp::set_message_handler(std::function<void(const json&)> handler) {
    message_handler = handler;
}

void ChatClientApp::set_connection_handler(std::function<void(bool)> handler) {
    connection_handler = handler;
}

bool ChatClientApp::wait_for_message(const std::string& type, int timeout_ms) {
    std::unique_lock<std::mutex> lock(messages_mutex);
    return message_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this, &type]() {
        for (const auto& msg : received_messages) {
            if (msg.contains("type") && msg["type"] == type) {
                return true;
            }
        }
        return false;
    });
}

bool ChatClientApp::wait_for_connection(int timeout_ms) {
    std::unique_lock<std::mutex> lock(connection_mutex);
    bool result = connection_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this]() {
        return connected.load();
    });
    
    if (!result) {
        std::cerr << "Connection timeout after " << timeout_ms << "ms" << std::endl;
    }
    
    return result;
}

void ChatClientApp::on_message(websocketpp::connection_hdl hdl, websocketpp::config::asio_client::message_type::ptr msg) {
    try {
        if (!msg || should_stop || !connected) return;
        
        auto j = json::parse(msg->get_payload());
        
        {
            std::lock_guard<std::mutex> lock(messages_mutex);
            received_messages.push_back(j);
        }
        
        // Update internal state based on message type
        if (j["type"] == "joined") {
            current_channel = j["channel"];
            if (!j["username"].empty()) {
                username = j["username"];
            }
        } else if (j["type"] == "channels") {
            last_channels.clear();
            for (const auto& ch : j["channels"]) {
                last_channels.push_back(ch);
            }
        }
        
        if (message_handler && !should_stop && connected) {
            message_handler(j);
        }
        
        message_cv.notify_all();
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse message: " << e.what() << std::endl;
    }
}

void ChatClientApp::on_open(websocketpp::connection_hdl hdl) {
    if (should_stop) return;
    
    connected = true;
    connection_cv.notify_all();
    if (connection_handler && !should_stop) {
        connection_handler(true);
    }
}

void ChatClientApp::on_close(websocketpp::connection_hdl hdl) {
    connected = false;
    if (connection_handler && !should_stop) {
        connection_handler(false);
    }
}

void ChatClientApp::on_fail(websocketpp::connection_hdl hdl) {
    connected = false;
    if (connection_handler && !should_stop) {
        connection_handler(false);
    }
} 