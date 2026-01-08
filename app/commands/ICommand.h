#ifndef APP_COMMANDS_ICOMMAND_H
#define APP_COMMANDS_ICOMMAND_H

#include "domains/ids/Ids.h"

#include <chrono>
#include <cstdint>
#include <expected>
#include <nlohmann/json.hpp>
#include <string>
#include <variant>
#include <vector>

namespace app {

// ------------------- command input ----------------

struct JsonInput {
    GlobalConnId conn;
    nlohmann::json body;
};

struct ConnectEvent {
    GlobalConnId conn;
    UserId user_id;
};

struct DisconnectEvent {
    GlobalConnId conn;
    int code{};
    std::string reason;
};

using CommandInput = std::variant<JsonInput, ConnectEvent, DisconnectEvent>;

// ---------------- outbound intents ----------------

struct Unicast {
    GlobalConnId conn;
    nlohmann::json payload;
};

struct Fanout {
    std::vector<GlobalConnId> conns;
    nlohmann::json payload;
};

struct AuthStateIntent {
    GlobalConnId conn;
    std::chrono::system_clock::time_point expires_at{};
    bool authenticated{false};
};

using OutboundIntent = std::variant<Unicast, Fanout, AuthStateIntent>;

// ---------------- command result ----------------

struct CommandError {
    std::string code;
    std::string message;
};

struct CommandSuccess {
    std::vector<OutboundIntent> intents;
};

using CommandResult = std::expected<CommandSuccess, CommandError>;

// ---------------- command interface ----------------

struct CommandContext;

class ICommand {
   public:
    virtual ~ICommand() = default;

    virtual CommandResult execute(CommandContext& ctx, const CommandInput cmd) = 0;
};

}  // namespace app

#endif  // APP_COMMANDS_ICOMMAND_H
