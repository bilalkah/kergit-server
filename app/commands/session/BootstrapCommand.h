#ifndef APP_SESSION_INIT_COMMAND_H
#define APP_SESSION_INIT_COMMAND_H

#include "app/commands/ICommand.h"
#include "app/dispatcher/CommandContext.h"

namespace app {

class BootstrapCommand : public ICommand {
   public:
    std::vector<net::outbound::OutgoingMessage> execute(CommandContext& ctx,
                                                        const queue::Event& evt) override;
};

}  // namespace app

#endif  // APP_SESSION_INIT_COMMAND_H
