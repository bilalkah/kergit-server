#pragma once
#include "server/MessageFilters.h"
#include "server/commands/ICommand.h"

#include <unordered_map>

class ChatCommand : public ICommand {
   public:
    using WsToUserMap = std::unordered_map<WS*, std::string>;
    ChatCommand(const WsToUserMap& ws_to_user) : ws_to_user(ws_to_user) {}
    void execute(json& j, User& user, ChatServerState& server, WS* ws) override {
        std::string text = j["text"];
        if (server.sendMessage(user.id, text)) {
            ChannelId channel = user.current_channel;
            auto& ch = server.channels[channel];

            // Get the timestamp from the last message in history
            auto timestamp = std::chrono::system_clock::now();
            // if (!ch.history.empty()) {
            //     timestamp = ch.history.back().timestamp;
            // }

            json resp;
            resp["type"] = "chat";
            resp["sender"] = user.username;
            resp["text"] = text;
            // Convert chrono time_point to epoch seconds for JSON
            auto epoch = timestamp.time_since_epoch();
            auto seconds = std::chrono::duration_cast<std::chrono::seconds>(epoch);
            resp["timestamp"] = seconds.count();
            for (const auto& uid : ch.member_user_ids) {
                for (const auto& [ws_ptr, ws_uid] : ws_to_user) {
                    if (ws_uid == uid) {
                        json out = resp;
                        send_json(ws_ptr, out, uWS::OpCode::TEXT);
                    }
                }
            }
        }
    }

   private:
    const WsToUserMap& ws_to_user;
};