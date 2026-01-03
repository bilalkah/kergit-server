#ifndef APP_COMMANDS_CHANNEL_RENAMECHANNELCOMMAND_H
#define APP_COMMANDS_CHANNEL_RENAMECHANNELCOMMAND_H

#include "app/commands/ICommand.h"

namespace app {

class RenameChannelCommand : public ICommand {
   public:
    CommandResult execute(CommandContext& ctx, const CommandInput cmd) override;

   private:
    static std::string sanitize(std::string name);
};

}  // namespace app

#endif  // APP_COMMANDS_CHANNEL_RENAMECHANNELCOMMAND_H
