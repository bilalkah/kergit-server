#ifndef APP_COMMANDS_MESSAGE_SENDMESSAGECOMMAND_H
#define APP_COMMANDS_MESSAGE_SENDMESSAGECOMMAND_H

#include "app/commands/ICommand.h"

namespace app {

class SendMessageCommand : public ICommand {
   public:
    CommandResult execute(CommandContext& ctx, const CommandInput cmd) override;
};

}  // namespace app

#endif  // APP_COMMANDS_MESSAGE_SENDMESSAGECOMMAND_H
