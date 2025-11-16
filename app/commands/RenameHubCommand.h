#ifndef APP_COMMANDS_RENAMEHUBCOMMAND_H
#define APP_COMMANDS_RENAMEHUBCOMMAND_H

#include "app/commands/ICommand.h"

class PersistenceGateway;

namespace net {
class ClientGateway;
class ConnectionManager;
}

namespace app::services {
class HubPublisher;
class PublicIdService;
}

namespace app {

class RenameHubCommand : public ICommand {
   public:
    RenameHubCommand(PersistenceGateway& db, net::ClientGateway& gateway, net::ConnectionManager& connections,
                     app::services::HubPublisher& hub_publisher,
                     app::services::PublicIdService& ids);
    void execute(CommandContext&) override;

   private:
    static std::string sanitize(std::string name);
    bool is_owner(net::PerSocketData& psd, const HubId& hub_id);

    PersistenceGateway& db_;
    net::ClientGateway& gateway_;
    net::ConnectionManager& connections_;
    app::services::HubPublisher& hub_publisher_;
    app::services::PublicIdService& ids_;
};

}  // namespace app

#endif  // APP_COMMANDS_RENAMEHUBCOMMAND_H
