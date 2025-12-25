#ifndef APP_COMMANDS_CREATEHUBCOMMAND_H
#define APP_COMMANDS_CREATEHUBCOMMAND_H

#include "app/commands/ICommand.h"

#include <string>

namespace app {

class CreateHubCommand : public ICommand {
   public:
    CreateHubCommand(ServiceObjects& svc_objs);
    void execute(CommandContext&) override;

   private:
    static std::string sanitize_name(std::string name);

    ServiceObjects& services_;
};

}  // namespace app

#endif  // APP_COMMANDS_CREATEHUBCOMMAND_H
