#ifndef APP_COMMANDS_ICOMMAND_H
#define APP_COMMANDS_ICOMMAND_H

#include "domains/ids/Ids.h"
#include "proto/event/error.pb.h"

#include <expected>
#include <string>
#include <variant>
#include <vector>

namespace app {

// ------------------- command input ----------------
struct JsonInput {
    GlobalConnId conn;
    std::string body;
};

struct MessageEvent {
    GlobalConnId conn;
    std::string body;
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

using CommandInput = std::variant<JsonInput, MessageEvent, ConnectEvent, DisconnectEvent>;

// ---------------- outbound intents ----------------

struct Unicast {
    GlobalConnId conn;
    std::string payload;
};

struct BinaryUnicast {
    GlobalConnId conn;
    std::string payload;
};

struct Fanout {
    std::vector<GlobalConnId> conns;
    std::string payload;
};

struct BinaryFanout {
    std::vector<GlobalConnId> conns;
    std::string payload;
};

struct AuthStateIntent {
    GlobalConnId conn;
    std::chrono::system_clock::time_point expires_at{};
    bool authenticated{false};
};

struct DropConnectionIntent {
    GlobalConnId conn;
    int code{};
    std::string reason;
};

using OutboundIntent =
    std::variant<Unicast, BinaryUnicast, Fanout, BinaryFanout, AuthStateIntent,
                 DropConnectionIntent>;

// ---------------- command result ----------------

struct CommandError {
    int code;
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
