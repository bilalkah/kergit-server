#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <iostream>
#include <string>
#include <sstream>
#include <nlohmann/json.hpp>

using websocketpp::connection_hdl;
typedef websocketpp::client<websocketpp::config::asio_client> client;
using json = nlohmann::json;

std::string username;
std::string current_channel;
client::connection_ptr con;

void print_help() {
    std::cout << "\nCommands:\n"
              << "/send <message>   - Send a chat message\n"
              << "/list            - List all channels\n"
              << "/join <channel>  - Join or create a channel\n"
              << "/help            - Show this help message\n"
              << "(Just type a message to send it)\n";
}

void on_message(client* c, connection_hdl hdl, websocketpp::config::asio_client::message_type::ptr msg) {
    try {
        auto j = json::parse(msg->get_payload());
        if (j["type"] == "chat") {
            std::cout << j["sender"] << ": " << j["text"] << std::endl;
        } else if (j["type"] == "joined") {
            current_channel = j["channel"];
            std::cout << "[Joined channel: " << j["channel"] << "] as '" << j["username"] << "'" << std::endl;
        } else if (j["type"] == "channels") {
            std::cout << "[Channels]: ";
            for (const auto& ch : j["channels"]) {
                std::cout << ch << " ";
            }
            std::cout << std::endl;
        }
    } catch (...) {
        std::cout << "[Received] " << msg->get_payload() << std::endl;
    }
}

int main() {
    client c;
    std::string uri = "ws://localhost:9001";

    std::cout << "Enter user name: ";
    std::getline(std::cin, username);

    std::cout << "Enter channel to join: ";
    std::getline(std::cin, current_channel);

    try {
        c.set_access_channels(websocketpp::log::alevel::none);
        c.clear_access_channels(websocketpp::log::alevel::frame_payload);
        c.init_asio();
        c.set_message_handler(std::bind(&on_message, &c, std::placeholders::_1, std::placeholders::_2));

        websocketpp::lib::error_code ec;
        con = c.get_connection(uri, ec);
        if (ec) {
            std::cout << "Could not create connection: " << ec.message() << std::endl;
            return 1;
        }

        c.connect(con);

        // Run the ASIO io_service on a separate thread
        std::thread t([&c]() { c.run(); });

        // Wait for connection to be established
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // Send initial join message
        json join_msg = { {"type", "join"}, {"channel", current_channel}, {"username", username} };
        con->send(join_msg.dump());

        print_help();
        std::string line;
        while (std::getline(std::cin, line)) {
            if (line.empty()) continue;
            if (line[0] == '/') {
                std::istringstream iss(line);
                std::string cmd;
                iss >> cmd;
                if (cmd == "/send") {
                    std::string msg;
                    std::getline(iss, msg);
                    if (!msg.empty() && msg[0] == ' ') msg = msg.substr(1);
                    if (!msg.empty()) {
                        json chat_msg = { {"type", "chat"}, {"text", msg} };
                        con->send(chat_msg.dump());
                    }
                } else if (cmd == "/list") {
                    json list_req = { {"type", "list"} };
                    con->send(list_req.dump());
                } else if (cmd == "/join") {
                    std::string new_channel;
                    iss >> new_channel;
                    if (!new_channel.empty()) {
                        current_channel = new_channel;
                        json join_msg = { {"type", "join"}, {"channel", current_channel}, {"username", username} };
                        con->send(join_msg.dump());
                    } else {
                        std::cout << "Usage: /join <channel>" << std::endl;
                    }
                } else if (cmd == "/help") {
                    print_help();
                } else {
                    std::cout << "Unknown command. Type /help for help." << std::endl;
                }
            } else {
                // Default: send as chat message
                json chat_msg = { {"type", "chat"}, {"text", line} };
                con->send(chat_msg.dump());
            }
        }

        t.join();
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
    }
    return 0;
} 