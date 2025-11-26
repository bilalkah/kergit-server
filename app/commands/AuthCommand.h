#ifndef APP_COMMANDS_AUTHCOMMAND_H
#define APP_COMMANDS_AUTHCOMMAND_H

#include "app/commands/ICommand.h"
#include "app/services/AuthService.h"
#include "app/services/HubPublisher.h"
#include "app/services/PublicIdService.h"
#include "domains/Channel.h"
#include "domains/Hub.h"
#include "domains/ids/Ids.h"

#include <nlohmann/json.hpp>
#include <unordered_map>
#include <vector>

class PersistenceGateway;

namespace net {
class ClientGateway;
class ConnectionManager;
}  // namespace net

namespace app {

class AuthCommand : public ICommand {
   public:
    AuthCommand(PersistenceGateway& db, net::ClientGateway& gateway,
                net::ConnectionManager& connections, app::services::HubPublisher& hub_publisher,
                app::services::PublicIdService& ids);
    void execute(CommandContext&) override;

   private:
    nlohmann::json build_bootstrap_payload(
        const std::vector<Hub>& hubs,
        const std::unordered_map<HubId, std::vector<Channel>>& channels_by_hub,
        const nlohmann::json& online_by_hub, const UserId& current_user);
    nlohmann::json collect_online_members(const std::vector<Hub>& hubs);

    app::services::AuthService auth_service_;
    PersistenceGateway& db_;
    net::ClientGateway& gateway_;
    net::ConnectionManager& connections_;
    app::services::HubPublisher& hub_publisher_;
    app::services::PublicIdService& ids_;
};

}  // namespace app

#endif  // APP_COMMANDS_AUTHCOMMAND_H
