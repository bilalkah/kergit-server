#pragma once
#include "ICommand.h"
#include <unordered_map>

class ChatCommand : public ICommand {
public:
    using WsToUserMap = std::unordered_map<uWS::WebSocket<false, true, struct PerSocketData>*, std::string>;
    ChatCommand(const WsToUserMap& ws_to_user) : ws_to_user(ws_to_user) {}
    void execute(json& j, User& user, ChatServer& server, uWS::WebSocket<false, true, struct PerSocketData>* ws) override {
        std::string text = j["text"];
        if (server.sendMessage(user.id, text)) {
            std::string channel = user.current_channel;
            auto& ch = server.channels[channel];
            json resp = { {"type", "chat"}, {"sender", user.username}, {"text", text} };
            std::string msg = resp.dump();
            for (const auto& uid : ch.user_ids) {
                for (const auto& [ws_ptr, ws_uid] : ws_to_user) {
                    if (ws_uid == uid) {
                        ws_ptr->send(msg);
                    }
                }
            }
        }
    }
private:
    const WsToUserMap& ws_to_user;
}; 