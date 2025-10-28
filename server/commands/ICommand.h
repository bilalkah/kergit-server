#pragma once
#include "common/ChatServer.h"
#include "domains/User.h"
#include "server/Config.h"

#include <App.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class ICommand {
   public:
    virtual ~ICommand() = default;
    virtual void execute(json&, User&, ChatServerState&, WS*) = 0;
};