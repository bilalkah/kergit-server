#ifndef APP_COMMANDS_UPDATEMEMBERROLECOMMAND_H
#define APP_COMMANDS_UPDATEMEMBERROLECOMMAND_H

#include "app/commands/ICommand.h"

class PersistenceGateway;

namespace net {
class ClientGateway;
class ConnectionManager;
}  // namespace net

namespace app::services {
class PublicIdService;
class HubPublisher;
}  // namespace app::services

namespace app {

class UpdateMemberRoleCommand : public ICommand {
   public:
    UpdateMemberRoleCommand(PersistenceGateway& db, net::ClientGateway& gateway,
                            net::ConnectionManager& connections,
                            app::services::PublicIdService& ids,
                            app::services::HubPublisher& hub_publisher);
    void execute(CommandContext&) override;

   private:
    bool is_owner(const CommandContext& ctx, const HubId& hub_id);

    PersistenceGateway& db_;
    net::ClientGateway& gateway_;
    net::ConnectionManager& connections_;
    app::services::PublicIdService& ids_;
    app::services::HubPublisher& hub_publisher_;
};

}  // namespace app

#endif  // APP_COMMANDS_UPDATEMEMBERROLECOMMAND_H
