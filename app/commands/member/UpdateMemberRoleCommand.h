#ifndef APP_COMMANDS_MEMBER_UPDATEMEMBERROLECOMMAND_H
#define APP_COMMANDS_MEMBER_UPDATEMEMBERROLECOMMAND_H

#include "app/commands/ICommand.h"

namespace app {

class UpdateMemberRoleCommand : public ICommand {
   public:
    CommandResult execute(CommandContext& ctx, const CommandInput cmd) override;
};

}  // namespace app

#endif  // APP_COMMANDS_MEMBER_UPDATEMEMBERROLECOMMAND_H
