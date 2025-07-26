#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <vector>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>

// Include WebRTC manager for voice calls
#include "WebRTCManager.h"

using websocketpp::connection_hdl;
typedef websocketpp::client<websocketpp::config::asio_client> client;
using json = nlohmann::json;

class ChatClientApp {
   public:
    ChatClientApp(const std::string& server_uri = "ws://localhost:9001");
    ~ChatClientApp();

    // Connection management
    bool connect();
    void disconnect();
    bool is_connected() const { return connected; }
    void reset();

    // Chat operations
    bool join_channel(const std::string& channel, const std::string& username = "");
    bool leave_channel();
    bool send_message(const std::string& text);
    bool list_channels();
    bool list_users();
    bool ping();

    // Voice call operations
    bool initiate_voice_call(const std::string& target_user);
    bool accept_voice_call(const std::string& call_id);
    bool reject_voice_call(const std::string& call_id);
    bool end_voice_call(const std::string& call_id);
    bool is_in_voice_call() const;
    WebRTCState get_voice_call_state() const;

    // Message handling
    void set_message_handler(std::function<void(const json&)> handler);
    void set_connection_handler(std::function<void(bool)> handler);
    void set_voice_call_handler(
        std::function<void(const std::string&, const std::string&)> handler);

    // Getters
    const std::string& get_username() const { return username; }
    const std::string& get_current_channel() const { return current_channel; }
    const std::vector<std::string>& get_last_channels() const { return last_channels; }

    // Wait utilities (for testing)
    bool wait_for_message(const std::string& type, int timeout_ms = 5000);
    bool wait_for_connection(int timeout_ms = 5000);

   private:
    void on_message(websocketpp::connection_hdl hdl,
                    websocketpp::config::asio_client::message_type::ptr msg);
    void on_open(websocketpp::connection_hdl hdl);
    void on_close(websocketpp::connection_hdl hdl);
    void on_fail(websocketpp::connection_hdl hdl);

    // Voice call message handling
    void handle_voice_call_message(const json& message);
    void handle_webrtc_signal(const json& message);

    std::string server_uri;
    std::string username;
    std::string current_channel;
    std::vector<std::string> last_channels;

    client ws_client;
    websocketpp::connection_hdl connection;
    std::thread client_thread;

    std::atomic<bool> connected{false};
    std::atomic<bool> should_stop{false};

    std::function<void(const json&)> message_handler;
    std::function<void(bool)> connection_handler;
    std::function<void(const std::string&, const std::string&)> voice_call_handler;

    std::mutex messages_mutex;
    std::condition_variable message_cv;
    std::vector<json> received_messages;

    std::mutex connection_mutex;
    std::condition_variable connection_cv;

    // WebRTC voice call manager
    WebRTCManager webrtc_manager;
    std::string current_call_id;
};