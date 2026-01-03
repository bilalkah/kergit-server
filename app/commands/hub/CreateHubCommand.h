#ifndef APP_COMMANDS_HUB_CREATEHUBCOMMAND_H
#define APP_COMMANDS_HUB_CREATEHUBCOMMAND_H

#include "app/commands/ICommand.h"

namespace app {

class CreateHubCommand : public ICommand {
   public:
    CommandResult execute(CommandContext& ctx, const CommandInput cmd) override;

   private:
    static std::string sanitize_name(std::string name);
};

}  // namespace app

#endif  // APP_COMMANDS_HUB_CREATEHUBCOMMAND_H
