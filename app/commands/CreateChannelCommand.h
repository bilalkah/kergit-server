#ifndef APP_COMMANDS_CREATECHANNELCOMMAND_H
#define APP_COMMANDS_CREATECHANNELCOMMAND_H

#include "app/commands/ICommand.h"
#include "domains/Hub.h"

#include <string>

namespace app {

class CreateChannelCommand : public ICommand {
   public:
    CreateChannelCommand(ServiceObjects& svc_objs);
    void execute(CommandContext&) override;

   private:
    bool has_privilege(const net::Snapshot& snapshot, const HubId& hub_id);

    ServiceObjects& services_;
};

}  // namespace app

#endif  // APP_COMMANDS_CREATECHANNELCOMMAND_H
