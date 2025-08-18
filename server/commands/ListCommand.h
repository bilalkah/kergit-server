#pragma once
#include "server/MessageFilters.h"
#include "server/commands/ICommand.h"

class ListCommand : public ICommand {
   public:
    void execute(json& j, User& user, ChatServerState& server, WS* ws) override {
        auto names = server.listChannels();
        json resp;
        resp["type"] = "channels";
        resp["channels"] = names;
        send_json(ws, resp, uWS::OpCode::TEXT);
    }
};