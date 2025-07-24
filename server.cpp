#include "App.h"
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct PerSocketData {
    std::string channel;
    std::string client_id;
    std::string username;
};

// channel name -> set of WebSocket pointers
std::unordered_map<std::string, std::unordered_set<uWS::WebSocket<false, true, PerSocketData>*>> channels;

int main() {
    uWS::App().ws<PerSocketData>("/*", {
        .open = [](auto *ws) {
            // Assign a unique client id (address as string)
            ws->getUserData()->client_id = std::to_string(reinterpret_cast<uintptr_t>(ws));
            std::cout << "Client connected! id=" << ws->getUserData()->client_id << std::endl;
        },
        .message = [](auto *ws, std::string_view message, uWS::OpCode opCode) {
            try {
                auto j = json::parse(message);
                if (j["type"] == "join") {
                    std::string channel = j["channel"];
                    std::string username = j.value("username", ws->getUserData()->client_id);
                    // Remove from previous channel if any
                    if (!ws->getUserData()->channel.empty()) {
                        auto &old_set = channels[ws->getUserData()->channel];
                        old_set.erase(ws);
                    }
                    ws->getUserData()->channel = channel;
                    ws->getUserData()->username = username;
                    channels[channel].insert(ws);
                    // Notify client joined
                    json resp = { {"type", "joined"}, {"channel", channel}, {"username", username} };
                    ws->send(resp.dump(), opCode);
                } else if (j["type"] == "chat") {
                    std::string channel = ws->getUserData()->channel;
                    if (!channel.empty()) {
                        json resp = {
                            {"type", "chat"},
                            {"sender", ws->getUserData()->username},
                            {"text", j["text"]}
                        };
                        std::string msg = resp.dump();
                        for (auto *client : channels[channel]) {
                            client->send(msg, opCode);
                        }
                    }
                } else if (j["type"] == "list") {
                    // Send list of channels to this client
                    json resp = { {"type", "channels"} };
                    std::vector<std::string> channel_names;
                    for (const auto &kv : channels) {
                        if (!kv.second.empty()) channel_names.push_back(kv.first);
                    }
                    resp["channels"] = channel_names;
                    ws->send(resp.dump(), opCode);
                }
            } catch (const std::exception &e) {
                std::cerr << "[ERROR] Invalid message: " << e.what() << std::endl;
            }
        },
        .close = [](auto *ws, int /*code*/, std::string_view /*message*/) {
            std::string channel = ws->getUserData()->channel;
            if (!channel.empty()) {
                channels[channel].erase(ws);
            }
            std::cout << "Client disconnected! id=" << ws->getUserData()->client_id << std::endl;
        }
    }).listen(9001, [](auto *token) {
        if (token) {
            std::cout << "Server listening on port 9001" << std::endl;
        } else {
            std::cout << "Failed to listen on port 9001" << std::endl;
        }
    }).run();
    return 0;
} 