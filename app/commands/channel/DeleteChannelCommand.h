#ifndef APP_COMMANDS_CHANNEL_DELETECHANNELCOMMAND_H
#define APP_COMMANDS_CHANNEL_DELETECHANNELCOMMAND_H

#include "app/commands/ICommand.h"

namespace app {

class DeleteChannelCommand : public ICommand {
   public:
    CommandResult execute(CommandContext& ctx, const CommandInput cmd) override;
};

}  // namespace app

#endif  // APP_COMMANDS_CHANNEL_DELETECHANNELCOMMAND_H
