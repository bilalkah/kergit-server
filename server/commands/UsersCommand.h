#pragma once
#include "ICommand.h"
#include "server/MessageFilters.h"

class UsersCommand : public ICommand {
   public:
    void execute(json& j, User& user, ChatServerState& server,
                 uWS::WebSocket<false, true, struct PerSocketData>* ws) override {
        std::string channel = user.current_channel;
        if (channel.empty()) {
            json resp;
            resp["type"] = "error";
            resp["message"] = "Not in any channel";
            send_json(ws, resp, uWS::OpCode::TEXT);
            return;
        }

        auto ch_it = server.channels.find(channel);
        if (ch_it == server.channels.end()) {
            json resp;
            resp["type"] = "error";
            resp["message"] = "Channel not found";
            send_json(ws, resp, uWS::OpCode::TEXT);
            return;
        }

        std::vector<std::string> usernames;
        for (const auto& uid : ch_it->second.user_ids) {
            auto user_it = server.users.find(uid);
            if (user_it != server.users.end()) {
                usernames.push_back(user_it->second.username);
            }
        }

        json resp;
        resp["type"] = "users";
        resp["channel"] = channel;
        resp["users"] = usernames;
        send_json(ws, resp, uWS::OpCode::TEXT);
    }
};