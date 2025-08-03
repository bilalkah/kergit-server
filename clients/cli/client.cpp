#include "ChatClientApp.h"

#include <chrono>
#include <iostream>
#include <sstream>
#include <string>

using namespace std::chrono;

std::string format_timestamp(std::time_t timestamp) {
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%H:%M:%S", std::localtime(&timestamp));
    return std::string(buffer);
}

void print_help(bool connected = true) {
    std::cout << "\nCommands:\n"
              << "/send <message>   - Send a chat message\n"
              << "/list            - List all channels\n"
              << "/users           - List users in current channel\n"
              << "/connect <channel> - Connect to an existing channel\n"
              << "/create <channel>  - Create and join a new channel\n"
              << "/disconnect      - Leave the channel (stay connected to server)\n"
              << "/quit           - Quit the app immediately\n"
              << "/ping           - Measure ping to server\n"
              << "/help           - Show this help message\n";
    std::cout << "(Just type a message to send it)\n";
}

int main() {
    ChatClientApp client;
    bool in_channel = false;

    std::cout << "Enter user name: ";
    std::string username;
    std::getline(std::cin, username);

    // Set up message handler
    client.set_message_handler([&client, &in_channel](const json& j) {
        if (j["type"] == "chat") {
            std::string timestamp_str = "";
            if (j.contains("timestamp")) {
                timestamp_str = "[" + format_timestamp(j["timestamp"]) + "] ";
            }
            std::cout << timestamp_str << j["sender"] << ": " << j["text"] << std::endl;
        } else if (j["type"] == "joined") {
            if (!j["channel"].empty()) {
                std::cout << "[Joined channel: " << j["channel"] << "] as '" << j["username"] << "'"
                          << std::endl;
                in_channel = true;
            } else {
                in_channel = false;
            }
        } else if (j["type"] == "channels") {
            std::cout << "[Channels]: ";
            for (const auto& ch : j["channels"]) {
                std::cout << ch << " ";
            }
            std::cout << std::endl;
        } else if (j["type"] == "users") {
            std::cout << "[Users in " << j["channel"] << "]: ";
            for (const auto& user : j["users"]) {
                std::cout << user << " ";
            }
            std::cout << std::endl;
        } else if (j["type"] == "user_joined") {
            std::cout << "[+] " << j["username"] << " joined the channel" << std::endl;
        } else if (j["type"] == "user_left") {
            std::cout << "[-] " << j["username"] << " left the channel" << std::endl;
        } else if (j["type"] == "user_disconnected") {
            std::cout << "[!] " << j["username"] << " disconnected" << std::endl;
        } else if (j["type"] == "error") {
            std::cout << "[ERROR] " << j["message"] << std::endl;
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
    });

    // Connect to server
    if (!client.connect()) {
        std::cout << "Failed to connect to server" << std::endl;
        return 1;
    }

    print_help(true);

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;

        if (line[0] == '/') {
            std::istringstream iss(line);
            std::string cmd;
            iss >> cmd;

            if (cmd == "/send") {
                if (!client.is_connected() || !in_channel) {
                    std::cout << "Not in a channel. Use /connect <channel> first." << std::endl;
                    continue;
                }
                std::string msg;
                std::getline(iss, msg);
                if (!msg.empty() && msg[0] == ' ') msg = msg.substr(1);
                if (!msg.empty()) {
                    client.send_message(msg);
                }
            } else if (cmd == "/list") {
                client.list_channels();
            } else if (cmd == "/users") {
                if (!client.is_connected() || !in_channel) {
                    std::cout << "Not in a channel. Use /connect <channel> first." << std::endl;
                    continue;
                }
                client.list_users();
            } else if (cmd == "/connect") {
                std::string new_channel;
                iss >> new_channel;
                if (!new_channel.empty()) {
                    // Check if channel exists in last_channels
                    const auto& channels = client.get_last_channels();
                    if (std::find(channels.begin(), channels.end(), new_channel) ==
                        channels.end()) {
                        std::cout << "[Warning] Channel '" << new_channel
                                  << "' does not exist. Use /list to see available channels."
                                  << std::endl;
                        continue;
                    }
                    client.join_channel(new_channel, username);
                } else {
                    std::cout << "Usage: /connect <channel>" << std::endl;
                }
            } else if (cmd == "/create") {
                std::string new_channel;
                iss >> new_channel;
                if (!new_channel.empty()) {
                    // Check if channel already exists
                    const auto& channels = client.get_last_channels();
                    if (std::find(channels.begin(), channels.end(), new_channel) !=
                        channels.end()) {
                        std::cout << "[Warning] Channel '" << new_channel
                                  << "' already exists. Use /connect <channel> to join."
                                  << std::endl;
                        continue;
                    }
                    client.join_channel(new_channel, username);
                } else {
                    std::cout << "Usage: /create <channel>" << std::endl;
                }
            } else if (cmd == "/disconnect") {
                if (!client.is_connected()) {
                    std::cout << "Already disconnected from server." << std::endl;
                    continue;
                }
                if (!in_channel) {
                    std::cout << "Not in a channel." << std::endl;
                    continue;
                }
                client.leave_channel();
                in_channel = false;
                std::cout << "[Left the channel. Use /connect <channel> to join another.]"
                          << std::endl;
            } else if (cmd == "/ping") {
                if (!client.is_connected()) {
                    std::cout << "Not connected." << std::endl;
                    continue;
                }
                client.ping();
            } else if (cmd == "/help") {
                print_help(client.is_connected());
            } else if (cmd == "/quit") {
                break;
            } else {
                std::cout << "Unknown command. Type /help for help." << std::endl;
            }
        } else {
            if (!client.is_connected() || !in_channel) {
                std::cout << "Not in a channel. Use /connect <channel> first." << std::endl;
                continue;
            }
            client.send_message(line);
        }
    }

    std::cout << "Quitting..." << std::endl;
    return 0;
}