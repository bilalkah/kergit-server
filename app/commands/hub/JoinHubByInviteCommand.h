#ifndef APP_COMMANDS_HUB_JOINHUBBYINVITECOMMAND_H
#define APP_COMMANDS_HUB_JOINHUBBYINVITECOMMAND_H

#include "app/commands/ICommand.h"

namespace app {

class JoinHubByInviteCommand : public ICommand {
   public:
    CommandResult execute(CommandContext& ctx, const CommandInput cmd) override;
};

}  // namespace app

#endif  // APP_COMMANDS_HUB_JOINHUBBYINVITECOMMAND_H
