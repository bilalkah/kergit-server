#ifndef APP_COMMANDS_JOINCHANNELCOMMAND_H
#define APP_COMMANDS_JOINCHANNELCOMMAND_H

#include "app/commands/ICommand.h"

namespace app {

class JoinChannelCommand : public ICommand {
   public:
    CommandResult execute(CommandContext& ctx, const CommandInput cmd) override;
};

}  // namespace app

#endif  // APP_COMMANDS_JOINCHANNELCOMMAND_H
