#ifndef APP_COMMANDS_HUB_UPDATEHUBCOMMAND_H
#define APP_COMMANDS_HUB_UPDATEHUBCOMMAND_H

#include "app/commands/ICommand.h"

namespace app {

class UpdateHubCommand : public ICommand {
   public:
    std::vector<net::outbound::OutgoingMessage> execute(CommandContext& ctx,
                                                        const queue::Event& evt) override;

   private:
    static std::string sanitize(std::string value);
};

}  // namespace app

#endif  // APP_COMMANDS_HUB_UPDATEHUBCOMMAND_H
