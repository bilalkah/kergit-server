#ifndef APP_COMMANDS_HUB_DELETEHUBCOMMAND_H
#define APP_COMMANDS_HUB_DELETEHUBCOMMAND_H

#include "app/commands/ICommand.h"

namespace app {

class DeleteHubCommand : public ICommand {
   public:
    CommandResult execute(CommandContext& ctx, const CommandInput cmd) override;
};

}  // namespace app

#endif  // APP_COMMANDS_HUB_DELETEHUBCOMMAND_H
