#ifndef APP_COMMANDS_SYSTEM_DISCONNECTIONCOMMAND_H
#define APP_COMMANDS_SYSTEM_DISCONNECTIONCOMMAND_H

#include "app/commands/ICommand.h"
#include "app/dispatcher/CommandContext.h"

namespace app {

class DisconnectionCommand : public ICommand {
   public:
    CommandResult execute(CommandContext& ctx, const CommandInput cmd) override;
};

}  // namespace app

#endif  // APP_COMMANDS_SYSTEM_DISCONNECTIONCOMMAND_H
