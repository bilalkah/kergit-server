#ifndef APP_COMMANDS_GETHUBINVITECOMMAND_H
#define APP_COMMANDS_GETHUBINVITECOMMAND_H

#include "app/commands/ICommand.h"

namespace app {

class GetHubInviteCommand : public ICommand {
   public:
    GetHubInviteCommand(ServiceObjects& svc_objs);
    void execute(CommandContext&) override;

   private:
    bool has_privilege(const CommandContext& ctx, const HubId& hub_id);

    ServiceObjects& services_;
};

}  // namespace app

#endif  // APP_COMMANDS_GETHUBINVITECOMMAND_H
