#ifndef APP_COMMANDS_CREATECHANNELCOMMAND_H
#define APP_COMMANDS_CREATECHANNELCOMMAND_H

#include "app/commands/ICommand.h"
#include "domains/Hub.h"

#include <string>

class ChatDB;

namespace app::services {
class HubPublisher;
}

namespace app {

class CreateChannelCommand : public ICommand {
   public:
    CreateChannelCommand(ChatDB& db, app::services::HubPublisher& hub_publisher);
    void execute(CommandContext&) override;

   private:
    bool has_privilege(net::PerSocketData& psd, const HubId& hub_id);

    ChatDB& db_;
    app::services::HubPublisher& hub_publisher_;
};

}  // namespace app

#endif  // APP_COMMANDS_CREATECHANNELCOMMAND_H
