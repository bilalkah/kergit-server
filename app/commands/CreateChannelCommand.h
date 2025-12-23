#ifndef APP_COMMANDS_CREATECHANNELCOMMAND_H
#define APP_COMMANDS_CREATECHANNELCOMMAND_H

#include "app/commands/ICommand.h"
#include "domains/Hub.h"

#include <string>

class PersistenceGateway;

namespace app::services {
class HubPublisher;
class PublicIdService;
}  // namespace app::services

namespace app {

class CreateChannelCommand : public ICommand {
   public:
    CreateChannelCommand(PersistenceGateway& db, app::services::HubPublisher& hub_publisher,
                         app::services::PublicIdService& ids);
    void execute(CommandContext&) override;

   private:
    bool has_privilege(const net::Snapshot& snapshot, const HubId& hub_id);

    PersistenceGateway& db_;
    app::services::HubPublisher& hub_publisher_;
    app::services::PublicIdService& ids_;
};

}  // namespace app

#endif  // APP_COMMANDS_CREATECHANNELCOMMAND_H
