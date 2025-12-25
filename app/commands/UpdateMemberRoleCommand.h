#ifndef APP_COMMANDS_UPDATEMEMBERROLECOMMAND_H
#define APP_COMMANDS_UPDATEMEMBERROLECOMMAND_H

#include "app/commands/ICommand.h"

namespace app {

class UpdateMemberRoleCommand : public ICommand {
   public:
    UpdateMemberRoleCommand(ServiceObjects& svc_objs);
    void execute(CommandContext&) override;

   private:
    bool is_owner(const CommandContext& ctx, const HubId& hub_id);

    ServiceObjects& services_;
};

}  // namespace app

#endif  // APP_COMMANDS_UPDATEMEMBERROLECOMMAND_H
