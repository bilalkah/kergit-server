#ifndef APP_COMMANDS_BOOTSTRAPCOMMAND_H
#define APP_COMMANDS_BOOTSTRAPCOMMAND_H

#include "app/commands/ICommand.h"
#include "app/dispatcher/CommandContext.h"

namespace app {

class BootstrapCommand : public ICommand {
   public:
    CommandResult execute(CommandContext& ctx, const CommandInput cmd) override;
};

}  // namespace app

#endif  // APP_COMMANDS_BOOTSTRAPCOMMAND_H
