#pragma once
#include "common/ChatServer.h"
#include "common/User.h"

#include <App.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class ICommand {
   public:
    virtual ~ICommand() = default;
    virtual void execute(json& j, User& user, ChatServerState& server,
                         uWS::WebSocket<false, true, struct PerSocketData>* ws) = 0;
};