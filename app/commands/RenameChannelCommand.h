#ifndef APP_COMMANDS_RENAMECHANNELCOMMAND_H
#define APP_COMMANDS_RENAMECHANNELCOMMAND_H

#include "app/commands/ICommand.h"

namespace app {

class RenameChannelCommand : public ICommand {
   public:
    RenameChannelCommand(ServiceObjects& svc_objs);
    void execute(CommandContext&) override;

   private:
    static std::string sanitize(std::string name);
    bool has_privilege(const CommandContext& ctx, const HubId& hub_id);

    ServiceObjects& services_;
};

}  // namespace app

#endif  // APP_COMMANDS_RENAMECHANNELCOMMAND_H
