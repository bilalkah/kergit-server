#ifndef APP_COMMANDS_HUB_UPDATEHUBMEMBERROLECOMMAND_H
#define APP_COMMANDS_HUB_UPDATEHUBMEMBERROLECOMMAND_H

#include "app/commands/ICommand.h"

namespace app {

class UpdateHubMemberRoleCommand : public ICommand {
   public:
    std::vector<net::outbound::OutgoingMessage> execute(CommandContext& ctx,
                                                        const queue::Event& evt) override;
};

}  // namespace app

#endif  // APP_COMMANDS_HUB_UPDATEHUBMEMBERROLECOMMAND_H
