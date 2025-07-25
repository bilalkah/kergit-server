#pragma once
#include "ICommand.h"

class PingCommand : public ICommand {
public:
    void execute(json& j, User&, ChatServer&, uWS::WebSocket<false, true, struct PerSocketData>* ws) override {
        json resp = { {"type", "pong"}, {"timestamp", j["timestamp"]} };
        ws->send(resp.dump());
    }
}; 