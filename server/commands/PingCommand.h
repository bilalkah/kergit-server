#pragma once
#include "server/MessageFilters.h"
#include "server/commands/ICommand.h"
#include <chrono>

class PingCommand : public ICommand {
   public:
    void execute(json& j, User&, ChatServerState&, WS* ws) override {
        json resp;
        resp["type"] = "pong";
        resp["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
        send_json(ws, resp, uWS::OpCode::TEXT);
    }
};