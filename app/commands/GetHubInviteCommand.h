#ifndef APP_COMMANDS_GETHUBINVITECOMMAND_H
#define APP_COMMANDS_GETHUBINVITECOMMAND_H

#include "app/commands/ICommand.h"

class PersistenceGateway;

namespace app::services {
class PublicIdService;
}

namespace app {

class GetHubInviteCommand : public ICommand {
   public:
    GetHubInviteCommand(PersistenceGateway& db, app::services::PublicIdService& ids);
    void execute(CommandContext&) override;

   private:
    bool has_privilege(net::PerSocketData& psd, const HubId& hub_id);

    PersistenceGateway& db_;
    app::services::PublicIdService& ids_;
};

}  // namespace app

#endif  // APP_COMMANDS_GETHUBINVITECOMMAND_H
