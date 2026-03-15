#ifndef APP_COMMANDS_SESSION_REQUESTSTATESYNCCOMMAND_H
#define APP_COMMANDS_SESSION_REQUESTSTATESYNCCOMMAND_H

#include "app/commands/ICommand.h"

namespace app {

class RequestStateSyncCommand : public ICommand {
   public:
    std::vector<net::outbound::OutgoingMessage> execute(CommandContext& ctx,
                                                        const queue::Event& evt) override;
};

}  // namespace app

#endif  // APP_COMMANDS_SESSION_REQUESTSTATESYNCCOMMAND_H
