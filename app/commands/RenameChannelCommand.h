#ifndef APP_COMMANDS_RENAMECHANNELCOMMAND_H
#define APP_COMMANDS_RENAMECHANNELCOMMAND_H

#include "app/commands/ICommand.h"

class PersistenceGateway;

namespace net {
class ClientGateway;
}

namespace app::services {
class HubPublisher;
class PublicIdService;
}  // namespace app::services

namespace app {

class RenameChannelCommand : public ICommand {
   public:
    RenameChannelCommand(PersistenceGateway& db, net::ClientGateway& gateway,
                         app::services::HubPublisher& hub_publisher,
                         app::services::PublicIdService& ids);
    void execute(CommandContext&) override;

   private:
    static std::string sanitize(std::string name);
    bool has_privilege(net::PerSocketData& psd, const HubId& hub_id);

    PersistenceGateway& db_;
    net::ClientGateway& gateway_;
    app::services::HubPublisher& hub_publisher_;
    app::services::PublicIdService& ids_;
};

}  // namespace app

#endif  // APP_COMMANDS_RENAMECHANNELCOMMAND_H
