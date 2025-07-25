#pragma once
#include <nlohmann/json.hpp>
#include "common/User.h"
#include "common/ChatServer.h"
#include <App.h>

using json = nlohmann::json;

class ICommand {
public:
    virtual ~ICommand() = default;
    virtual void execute(json& j, User& user, ChatServerState& server, uWS::WebSocket<false, true, struct PerSocketData>* ws) = 0;
}; 