#ifndef APP_COMMANDS_HUB_VALIDATEHUBINVITECOMMAND_H
#define APP_COMMANDS_HUB_VALIDATEHUBINVITECOMMAND_H

#include "app/commands/ICommand.h"

namespace app {

class ValidateHubInviteCommand : public ICommand {
   public:
    std::vector<net::outbound::OutgoingMessage> execute(CommandContext& ctx,
                                                        const queue::Event& evt) override;
};

}  // namespace app

#endif  // APP_COMMANDS_HUB_VALIDATEHUBINVITECOMMAND_H
