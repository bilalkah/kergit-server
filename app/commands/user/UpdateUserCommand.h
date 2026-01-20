#ifndef APP_COMMANDS_USER_UPDATEUSERCOMMAND_H
#define APP_COMMANDS_USER_UPDATEUSERCOMMAND_H

#include "app/commands/ICommand.h"

namespace app {

class UpdateUserCommand : public ICommand {
   public:
    std::vector<net::outbound::OutgoingMessage> execute(CommandContext& ctx,
                                                        const queue::Event& evt) override;

   private:
    static std::string sanitize(std::string value);
};

}  // namespace app

#endif  // APP_COMMANDS_USER_UPDATEUSERCOMMAND_H
