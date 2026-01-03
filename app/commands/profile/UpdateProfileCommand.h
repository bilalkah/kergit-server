#ifndef APP_COMMANDS_PROFILE_UPDATEPROFILECOMMAND_H
#define APP_COMMANDS_PROFILE_UPDATEPROFILECOMMAND_H

#include "app/commands/ICommand.h"

namespace app {

class UpdateProfileCommand : public ICommand {
   public:
    CommandResult execute(CommandContext& ctx, const CommandInput cmd) override;

   private:
    static std::string trim(std::string value);
};

}  // namespace app

#endif  // APP_COMMANDS_PROFILE_UPDATEPROFILECOMMAND_H
