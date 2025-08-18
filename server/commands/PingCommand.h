#pragma once
#include "server/MessageFilters.h"
#include "server/commands/ICommand.h"

class PingCommand : public ICommand {
   public:
    void execute(json& j, User&, ChatServerState&, WS* ws) override {
        json resp;
        resp["type"] = "pong";
        resp["timestamp"] = j["timestamp"];
        send_json(ws, resp, uWS::OpCode::TEXT);
    }
};