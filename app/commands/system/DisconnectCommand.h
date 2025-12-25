#ifndef APP_COMMANDS_SYSTEM_DISCONNECTCOMMAND_H
#define APP_COMMANDS_SYSTEM_DISCONNECTCOMMAND_H

#include "app/commands/ICommand.h"
#include "app/memory/OnMemoryCache.h"

#include <optional>
#include <string>

namespace app {

class DisconnectCommand : public ICommand {
   public:
    DisconnectCommand(ServiceObjects& svc_objs);
    void execute(CommandContext&) override;

   private:
    ServiceObjects& services_;
};

}  // namespace app

#endif  // APP_COMMANDS_SYSTEM_DISCONNECTCOMMAND_H
