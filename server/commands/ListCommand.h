#pragma once
#include "ICommand.h"
#include "server/MessageFilters.h"

class ListCommand : public ICommand {
   public:
    void execute(json& j, User& user, ChatServerState& server,
                 uWS::WebSocket<false, true, struct PerSocketData>* ws) override {
        auto names = server.listChannels();
        json resp;
        resp["type"] = "channels";
        resp["channels"] = names;
        send_json(ws, resp, uWS::OpCode::TEXT);
    }
};