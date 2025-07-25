#include "App.h"
#include <iostream>
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "common/User.h"
#include "common/Channel.h"
#include "common/Message.h"
#include "common/ChatServer.h"
#include "server/commands/AllCommands.h"

using json = nlohmann::json;

struct PerSocketData {
    std::string user_id;
};

// Map WebSocket pointer to user id
std::unordered_map<uWS::WebSocket<false, true, PerSocketData>*, std::string> ws_to_user;

ChatServer chatServer;

std::unordered_map<std::string, std::unique_ptr<ICommand>> command_map;

void setup_commands(const std::unordered_map<uWS::WebSocket<false, true, PerSocketData>*, std::string>& ws_to_user) {
    command_map["join"] = std::make_unique<JoinCommand>();
    command_map["chat"] = std::make_unique<ChatCommand>(ws_to_user);
    command_map["list"] = std::make_unique<ListCommand>();
    command_map["ping"] = std::make_unique<PingCommand>();
}

int main() {
    std::unordered_map<uWS::WebSocket<false, true, PerSocketData>*, std::string> ws_to_user;
    setup_commands(ws_to_user);
    uWS::App().ws<PerSocketData>("/*", {
        .open = [&ws_to_user](auto *ws) {
            std::string user_id = std::to_string(reinterpret_cast<uintptr_t>(ws));
            ws->getUserData()->user_id = user_id;
            User user;
            user.id = user_id;
            user.username = user_id;
            chatServer.users[user_id] = user;
            ws_to_user[ws] = user_id;
            std::cout << "Client connected! id=" << user_id << std::endl;
        },
        .message = [](auto *ws, std::string_view message, uWS::OpCode opCode) {
            try {
                auto j = json::parse(message);
                std::string user_id = ws->getUserData()->user_id;
                auto& user = chatServer.users[user_id];
                std::string type = j["type"];
                auto it = command_map.find(type);
                if (it != command_map.end()) {
                    it->second->execute(j, user, chatServer, ws);
                } else {
                    std::cerr << "[ERROR] Unknown command type: " << type << std::endl;
                }
            } catch (const std::exception &e) {
                std::cerr << "[ERROR] Invalid message: " << e.what() << std::endl;
            }
        },
        .close = [&ws_to_user](auto *ws, int /*code*/, std::string_view /*message*/) {
            std::string user_id = ws->getUserData()->user_id;
            chatServer.leaveChannel(user_id);
            chatServer.users.erase(user_id);
            ws_to_user.erase(ws);
            std::cout << "Client disconnected! id=" << user_id << std::endl;
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