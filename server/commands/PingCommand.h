#pragma once
#include "ICommand.h"
#include "server/MessageFilters.h"

class PingCommand : public ICommand {
   public:
    void execute(json& j, User&, ChatServerState&,
                 uWS::WebSocket<false, true, struct PerSocketData>* ws) override {
        json resp;
        resp["type"] = "pong";
        resp["timestamp"] = j["timestamp"];
        send_json(ws, resp, uWS::OpCode::TEXT);
    }
};