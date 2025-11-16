#ifndef APP_COMMANDS_UPDATEPROFILECOMMAND_H
#define APP_COMMANDS_UPDATEPROFILECOMMAND_H

#include "app/commands/ICommand.h"

#include <optional>
#include <string>

class PersistenceGateway;

namespace app::services {
class HubPublisher;
class PublicIdService;
}  // namespace app::services

namespace app {

class UpdateProfileCommand : public ICommand {
   public:
    UpdateProfileCommand(PersistenceGateway& db, app::services::HubPublisher& hub_publisher,
                         app::services::PublicIdService& ids);
    void execute(CommandContext&) override;

   private:
    static std::string trim(std::string value);

    PersistenceGateway& db_;
    app::services::HubPublisher& hub_publisher_;
    app::services::PublicIdService& ids_;
};

}  // namespace app

#endif  // APP_COMMANDS_UPDATEPROFILECOMMAND_H
