#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <iostream>
#include <string>
#include <sstream>
#include <nlohmann/json.hpp>
#include <chrono>
#include <vector>

using websocketpp::connection_hdl;
typedef websocketpp::client<websocketpp::config::asio_client> client;
using json = nlohmann::json;
using namespace std::chrono;
steady_clock::time_point last_ping_time;

std::string username;
std::string current_channel;
client::connection_ptr con;
std::vector<std::string> last_channels;

void print_help(bool connected = true) {
    std::cout << "\nCommands:\n"
              << "/send <message>   - Send a chat message\n"
              << "/list            - List all channels\n"
              << "/connect <channel> - Connect to an existing channel\n"
              << "/create <channel>  - Create and join a new channel\n"
              << "/disconnect      - Leave the channel (stay connected to server)\n"
              << "/quit           - Quit the app immediately\n"
              << "/ping           - Measure ping to server\n"
              << "/help           - Show this help message\n";
    std::cout << "(Just type a message to send it)\n";
}

void on_message(client* c, connection_hdl hdl, websocketpp::config::asio_client::message_type::ptr msg) {
    try {
        auto j = json::parse(msg->get_payload());
        if (j["type"] == "chat") {
            std::cout << j["sender"] << ": " << j["text"] << std::endl;
        } else if (j["type"] == "joined") {
            current_channel = j["channel"];
            if (!current_channel.empty()) {
                std::cout << "[Joined channel: " << j["channel"] << "] as '" << j["username"] << "'" << std::endl;
            }
        } else if (j["type"] == "channels") {
            std::cout << "[Channels]: ";
            last_channels.clear();
            for (const auto& ch : j["channels"]) {
                std::cout << ch << " ";
                last_channels.push_back(ch);
            }
            std::cout << std::endl;
        } else if (j["type"] == "pong") {
            if (j.contains("timestamp")) {
                auto now = steady_clock::now();
                auto sent = static_cast<steady_clock::rep>(j["timestamp"]);
                auto ms = duration_cast<milliseconds>(now.time_since_epoch()).count() - sent;
                std::cout << "[Ping] Round-trip time: " << ms << " ms" << std::endl;
            } else {
                std::cout << "[Ping] Pong received, but no timestamp." << std::endl;
            }
        }
    } catch (...) {
        std::cout << "[Received] " << msg->get_payload() << std::endl;
    }
}

int main() {
    client c;
    std::string uri = "ws://localhost:9001";
    bool connected = false;
    bool in_channel = false;
    std::thread t;
    websocketpp::lib::error_code ec;
    
    std::cout << "Enter user name: ";
    std::getline(std::cin, username);

    // Always connect to server at startup
    c.set_access_channels(websocketpp::log::alevel::none);
    c.clear_access_channels(websocketpp::log::alevel::frame_payload);
    c.init_asio();
    c.set_message_handler(std::bind(&on_message, &c, std::placeholders::_1, std::placeholders::_2));
    con = c.get_connection(uri, ec);
    if (ec) {
        std::cout << "Could not create connection: " << ec.message() << std::endl;
        return 1;
    }
    c.connect(con);
    t = std::thread([&c]() { c.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    connected = true;
    in_channel = false;
    print_help(true);

    auto request_channels = [&]() {
        if (!connected) return;
        json list_req = { {"type", "list"} };
        con->send(list_req.dump());
    };

    auto connect_channel = [&](const std::string& channel) {
        if (!connected) {
            std::cout << "Not connected to server." << std::endl;
            return;
        }
        // Check if channel exists in last_channels
        if (std::find(last_channels.begin(), last_channels.end(), channel) == last_channels.end()) {
            std::cout << "[Warning] Channel '" << channel << "' does not exist. Use /list to see available channels." << std::endl;
            return;
        }
        current_channel = channel;
        json join_msg = { {"type", "join"}, {"channel", current_channel}, {"username", username} };
        con->send(join_msg.dump());
        in_channel = true;
        std::cout << "[Connected to channel: " << channel << "]" << std::endl;
    };

    auto create_channel = [&](const std::string& channel) {
        if (!connected) {
            std::cout << "Not connected to server." << std::endl;
            return;
        }
        // Check if channel already exists
        if (std::find(last_channels.begin(), last_channels.end(), channel) != last_channels.end()) {
            std::cout << "[Warning] Channel '" << channel << "' already exists. Use /connect <channel> to join." << std::endl;
            return;
        }
        current_channel = channel;
        json join_msg = { {"type", "join"}, {"channel", current_channel}, {"username", username} };
        con->send(join_msg.dump());
        in_channel = true;
        std::cout << "[Created and joined channel: " << channel << "]" << std::endl;
    };

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        if (line[0] == '/') {
            std::istringstream iss(line);
            std::string cmd;
            iss >> cmd;
            if (cmd == "/send") {
                if (!connected || !in_channel) { std::cout << "Not in a channel. Use /connect <channel> first." << std::endl; continue; }
                std::string msg;
                std::getline(iss, msg);
                if (!msg.empty() && msg[0] == ' ') msg = msg.substr(1);
                if (!msg.empty()) {
                    json chat_msg = { {"type", "chat"}, {"text", msg} };
                    con->send(chat_msg.dump());
                }
            } else if (cmd == "/list") {
                request_channels();
            } else if (cmd == "/connect") {
                std::string new_channel;
                iss >> new_channel;
                if (!new_channel.empty()) {
                    connect_channel(new_channel);
                } else {
                    std::cout << "Usage: /connect <channel>" << std::endl;
                }
            } else if (cmd == "/create") {
                std::string new_channel;
                iss >> new_channel;
                if (!new_channel.empty()) {
                    create_channel(new_channel);
                } else {
                    std::cout << "Usage: /create <channel>" << std::endl;
                }
            } else if (cmd == "/disconnect") {
                if (!connected) { std::cout << "Already disconnected from server." << std::endl; continue; }
                if (!in_channel) { std::cout << "Not in a channel." << std::endl; continue; }
                // Leave the channel by sending join with empty channel
                json leave_msg = { {"type", "join"}, {"channel", ""}, {"username", username} };
                con->send(leave_msg.dump());
                in_channel = false;
                std::cout << "[Left the channel. Use /connect <channel> to join another.]" << std::endl;
            } else if (cmd == "/ping") {
                if (!connected) { std::cout << "Not connected." << std::endl; continue; }
                auto now = steady_clock::now();
                auto ms = duration_cast<milliseconds>(now.time_since_epoch()).count();
                json ping_msg = { {"type", "ping"}, {"timestamp", ms} };
                con->send(ping_msg.dump());
            } else if (cmd == "/help") {
                print_help(connected);
            } else if (cmd == "/quit") {
                if (connected) {
                    con->close(websocketpp::close::status::normal, "Client quit");
                    if (t.joinable()) t.join();
                }
                std::cout << "Quitting..." << std::endl;
                break;
            } else {
                std::cout << "Unknown command. Type /help for help." << std::endl;
            }
        } else {
            if (!connected || !in_channel) { std::cout << "Not in a channel. Use /connect <channel> first." << std::endl; continue; }
            json chat_msg = { {"type", "chat"}, {"text", line} };
            con->send(chat_msg.dump());
        }
    }
    if (connected && t.joinable()) t.join();
    return 0;
} 