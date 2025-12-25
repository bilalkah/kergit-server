#ifndef APP_COMMANDS_UPDATEPROFILECOMMAND_H
#define APP_COMMANDS_UPDATEPROFILECOMMAND_H

#include "app/commands/ICommand.h"

#include <optional>
#include <string>

namespace app {

class UpdateProfileCommand : public ICommand {
   public:
    UpdateProfileCommand(ServiceObjects& svc_objs);
    void execute(CommandContext&) override;

   private:
    static std::string trim(std::string value);

    ServiceObjects& services_;
};

}  // namespace app

#endif  // APP_COMMANDS_UPDATEPROFILECOMMAND_H
