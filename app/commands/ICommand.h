#ifndef APP_COMMANDS_ICOMMAND_H
#define APP_COMMANDS_ICOMMAND_H

#include "app/queue/Msg.h"
#include "domains/ids/Ids.h"
#include "net/outbound/Msg.h"

#include <chrono>
#include <expected>
#include <string>
#include <variant>
#include <vector>

namespace app {

// ---------------- command interface ----------------

struct CommandContext;

class ICommand {
   public:
    virtual ~ICommand() = default;

    virtual std::vector<net::outbound::OutgoingMessage> execute(CommandContext& ctx,
                                                                const queue::Event& evt) = 0;
};

}  // namespace app

#endif  // APP_COMMANDS_ICOMMAND_H
