#ifndef APP_COMMANDS_RENAMEHUBCOMMAND_H
#define APP_COMMANDS_RENAMEHUBCOMMAND_H

#include "app/commands/ICommand.h"

namespace app {

class RenameHubCommand : public ICommand {
   public:
    RenameHubCommand(ServiceObjects& svc_objs);
    void execute(CommandContext&) override;

   private:
    static std::string sanitize(std::string name);
    bool is_owner(const CommandContext& ctx, const HubId& hub_id);

    ServiceObjects& services_;
};

}  // namespace app

#endif  // APP_COMMANDS_RENAMEHUBCOMMAND_H
