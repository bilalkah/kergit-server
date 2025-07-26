#pragma once
#include "ICommand.h"

class PingCommand : public ICommand {
   public:
    void execute(json& j, User&, ChatServerState&,
                 uWS::WebSocket<false, true, struct PerSocketData>* ws) override {
        json resp;
        resp["type"] = "pong";
        resp["timestamp"] = j["timestamp"];
        ws->send(resp.dump());
    }
};