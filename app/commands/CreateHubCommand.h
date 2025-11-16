#ifndef APP_COMMANDS_CREATEHUBCOMMAND_H
#define APP_COMMANDS_CREATEHUBCOMMAND_H

#include "app/commands/ICommand.h"

#include <string>

class PersistenceGateway;

namespace net {
class ClientGateway;
}

namespace app::services {
class HubPublisher;
class PublicIdService;
}  // namespace app::services

namespace app {

class CreateHubCommand : public ICommand {
   public:
    CreateHubCommand(PersistenceGateway& db, net::ClientGateway& gateway,
                     app::services::HubPublisher& hub_publisher,
                     app::services::PublicIdService& ids);
    void execute(CommandContext&) override;

   private:
    static std::string sanitize_name(std::string name);

    PersistenceGateway& db_;
    net::ClientGateway& gateway_;
    app::services::HubPublisher& hub_publisher_;
    app::services::PublicIdService& ids_;
};

}  // namespace app

#endif  // APP_COMMANDS_CREATEHUBCOMMAND_H
