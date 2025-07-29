#include "ChatClientApp.h"

#include <chrono>
#include <iostream>

using namespace std::chrono;

ChatClientApp::ChatClientApp(const std::string& server_uri) : server_uri(server_uri) {
    ws_client.set_access_channels(websocketpp::log::alevel::none);
    ws_client.clear_access_channels(websocketpp::log::alevel::frame_payload);
    ws_client.init_asio();

    ws_client.set_message_handler(
        std::bind(&ChatClientApp::on_message, this, std::placeholders::_1, std::placeholders::_2));
    ws_client.set_open_handler(std::bind(&ChatClientApp::on_open, this, std::placeholders::_1));
    ws_client.set_close_handler(std::bind(&ChatClientApp::on_close, this, std::placeholders::_1));
    ws_client.set_fail_handler(std::bind(&ChatClientApp::on_fail, this, std::placeholders::_1));

    // Set up WebRTC manager callbacks
    webrtc_manager.setOnCallStateChange([](WebRTCState state) {
        std::cout << "[VOICE] Call state changed: " << static_cast<int>(state) << std::endl;
    });

    webrtc_manager.setOnOffer([this](const std::string& sdp) {
        // Send offer through signaling server
        json offer_msg;
        offer_msg["type"] = "webrtc_signal";
        offer_msg["signal_type"] = "offer";
        offer_msg["sdp"] = sdp;
        offer_msg["call_id"] = current_call_id;

        if (connected) {
            ws_client.send(connection, offer_msg.dump(), websocketpp::frame::opcode::text);
        }
    });

    webrtc_manager.setOnAnswer([this](const std::string& sdp) {
        // Send answer through signaling server
        json answer_msg;
        answer_msg["type"] = "webrtc_signal";
        answer_msg["signal_type"] = "answer";
        answer_msg["sdp"] = sdp;
        answer_msg["call_id"] = current_call_id;

        if (connected) {
            ws_client.send(connection, answer_msg.dump(), websocketpp::frame::opcode::text);
        }
    });

    webrtc_manager.setOnIceCandidate([this](const std::string& candidate) {
        // Send ICE candidate through signaling server
        json ice_msg;
        ice_msg["type"] = "webrtc_signal";
        ice_msg["signal_type"] = "ice_candidate";
        ice_msg["candidate"] = candidate;
        ice_msg["call_id"] = current_call_id;

        if (connected) {
            ws_client.send(connection, ice_msg.dump(), websocketpp::frame::opcode::text);
        }
    });
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

        ws_client.set_message_handler(std::bind(&ChatClientApp::on_message, this,
                                                std::placeholders::_1, std::placeholders::_2));
        ws_client.set_open_handler(std::bind(&ChatClientApp::on_open, this, std::placeholders::_1));
        ws_client.set_close_handler(
            std::bind(&ChatClientApp::on_close, this, std::placeholders::_1));
        ws_client.set_fail_handler(std::bind(&ChatClientApp::on_fail, this, std::placeholders::_1));
    } catch (const std::exception& e) {
        std::cerr << "Failed to reset WebSocket client: " << e.what() << std::endl;
    }
}

bool ChatClientApp::join_channel(const std::string& channel, const std::string& username) {
    if (!connected) return false;

    json msg = {{"type", "join"},
                {"channel", channel},
                {"username", username.empty() ? this->username : username}};

    ws_client.send(connection, msg.dump(), websocketpp::frame::opcode::text);
    current_channel = channel;
    return true;
}

bool ChatClientApp::leave_channel() {
    if (!connected) return false;

    json msg = {{"type", "join"}, {"channel", ""}, {"username", username}};

    ws_client.send(connection, msg.dump(), websocketpp::frame::opcode::text);
    current_channel.clear();
    return true;
}

bool ChatClientApp::send_message(const std::string& text) {
    if (!connected || current_channel.empty()) return false;

    json msg = {{"type", "chat"}, {"text", text}};

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

    json msg = {{"type", "ping"}, {"timestamp", ms}};

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
    bool result = connection_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                                         [this]() { return connected.load(); });

    if (!result) {
        std::cerr << "Connection timeout after " << timeout_ms << "ms" << std::endl;
    }

    return result;
}

void ChatClientApp::on_message(websocketpp::connection_hdl hdl,
                               websocketpp::config::asio_client::message_type::ptr msg) {
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

        // Handle voice call messages
        if (j["type"] == "call_incoming" || j["type"] == "call_accepted" ||
            j["type"] == "call_rejected" || j["type"] == "call_ended") {
            handle_voice_call_message(j);
        } else if (j["type"] == "webrtc_signal") {
            handle_webrtc_signal(j);
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

// Voice call operations
bool ChatClientApp::initiate_voice_call(const std::string& target_user) {
    if (!connected) {
        std::cout << "[VOICE] Not connected to server" << std::endl;
        return false;
    }

    if (is_in_voice_call()) {
        std::cout << "[VOICE] Already in a call" << std::endl;
        return false;
    }

    // Generate a call ID
    current_call_id = "call_" + std::to_string(std::time(nullptr));

    // Send call request to server
    json call_request;
    call_request["type"] = "call_request";
    call_request["target_user"] = target_user;
    call_request["media_type"] = "voice";
    call_request["call_id"] = current_call_id;

    ws_client.send(connection, call_request.dump(), websocketpp::frame::opcode::text);

    std::cout << "[VOICE] Initiating voice call to " << target_user << std::endl;
    return true;
}

bool ChatClientApp::accept_voice_call(const std::string& call_id) {
    if (!connected) {
        std::cout << "[VOICE] Not connected to server" << std::endl;
        return false;
    }

    current_call_id = call_id;

    // Send call accept to server
    json call_accept;
    call_accept["type"] = "call_accept";
    call_accept["call_id"] = call_id;

    ws_client.send(connection, call_accept.dump(), websocketpp::frame::opcode::text);

    std::cout << "[VOICE] Accepting call " << call_id << std::endl;
    return true;
}

bool ChatClientApp::reject_voice_call(const std::string& call_id) {
    if (!connected) {
        std::cout << "[VOICE] Not connected to server" << std::endl;
        return false;
    }

    // Send call reject to server
    json call_reject;
    call_reject["type"] = "call_reject";
    call_reject["call_id"] = call_id;

    ws_client.send(connection, call_reject.dump(), websocketpp::frame::opcode::text);

    std::cout << "[VOICE] Rejecting call " << call_id << std::endl;
    return true;
}

bool ChatClientApp::end_voice_call(const std::string& call_id) {
    if (!connected) {
        std::cout << "[VOICE] Not connected to server" << std::endl;
        return false;
    }

    // Send call end to server
    json call_end;
    call_end["type"] = "call_end";
    call_end["call_id"] = call_id;

    ws_client.send(connection, call_end.dump(), websocketpp::frame::opcode::text);

    // End the call locally
    webrtc_manager.endCall(call_id);
    current_call_id.clear();

    std::cout << "[VOICE] Ending call " << call_id << std::endl;
    return true;
}

bool ChatClientApp::is_in_voice_call() const { return webrtc_manager.isInCall(); }

WebRTCState ChatClientApp::get_voice_call_state() const { return webrtc_manager.getState(); }

void ChatClientApp::set_voice_call_handler(
    std::function<void(const std::string&, const std::string&)> handler) {
    voice_call_handler = handler;
}

void ChatClientApp::handle_voice_call_message(const json& message) {
    std::string msg_type = message["type"];

    if (msg_type == "call_incoming") {
        std::string from_user = message["from_user"];
        std::string call_id = message["call_id"];
        std::string media_type = message["media_type"];

        std::cout << "[VOICE] Incoming " << media_type << " call from " << from_user
                  << " (ID: " << call_id << ")" << std::endl;
        std::cout << "[VOICE] Type /accept " << call_id << " to accept or /reject " << call_id
                  << " to reject" << std::endl;

        if (voice_call_handler) {
            voice_call_handler("incoming", from_user);
        }
    } else if (msg_type == "call_accepted") {
        std::string call_id = message["call_id"];
        std::string accepted_by = message["accepted_by"];

        std::cout << "[VOICE] Call " << call_id << " accepted by " << accepted_by << std::endl;

        // Start WebRTC connection
        webrtc_manager.acceptCall(call_id);

    } else if (msg_type == "call_rejected") {
        std::string call_id = message["call_id"];
        std::string rejected_by = message["rejected_by"];

        std::cout << "[VOICE] Call " << call_id << " rejected by " << rejected_by << std::endl;

        // Clean up local call state
        webrtc_manager.rejectCall(call_id);
        current_call_id.clear();

    } else if (msg_type == "call_ended") {
        std::string call_id = message["call_id"];
        std::string ended_by = message["ended_by"];

        std::cout << "[VOICE] Call " << call_id << " ended by " << ended_by << std::endl;

        // End the call locally
        webrtc_manager.endCall(call_id);
        current_call_id.clear();
    }
}

void ChatClientApp::handle_webrtc_signal(const json& message) {
    std::string signal_type = message["signal_type"];
    std::string call_id = message["call_id"];
    std::string from_user = message["from_user"];

    if (signal_type == "offer") {
        std::string sdp = message["sdp"];
        std::cout << "[VOICE] Received WebRTC offer from " << from_user << std::endl;
        webrtc_manager.handleOffer(call_id, sdp);

    } else if (signal_type == "answer") {
        std::string sdp = message["sdp"];
        std::cout << "[VOICE] Received WebRTC answer from " << from_user << std::endl;
        webrtc_manager.handleAnswer(call_id, sdp);

    } else if (signal_type == "ice_candidate") {
        std::string candidate = message["candidate"];
        std::cout << "[VOICE] Received ICE candidate from " << from_user << std::endl;
        webrtc_manager.handleIceCandidate(call_id, candidate);
    }
}