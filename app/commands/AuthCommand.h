#ifndef APP_COMMANDS_AUTHCOMMAND_H
#define APP_COMMANDS_AUTHCOMMAND_H

#include "app/commands/ICommand.h"
#include "app/services/AuthService.h"
#include "app/services/HubPublisher.h"
#include "domains/Channel.h"
#include "domains/Hub.h"
#include "domains/ids/Ids.h"

#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

class ChatDB;

namespace net {
class ClientGateway;
class ConnectionManager;
}  // namespace net

namespace app {

class AuthCommand : public ICommand {
   public:
    AuthCommand(ChatDB& db, net::ClientGateway& gateway, net::ConnectionManager& connections,
                app::services::HubPublisher& hub_publisher);
    void execute(CommandContext&) override;

   private:
    void fill_psd(net::PerSocketData& psd, const infra::security::token::UserClaims& claims);
    void subscribe_to_hubs(const net::PerSocketData& psd, const std::vector<Hub>& hubs) const;
    nlohmann::json build_bootstrap_payload(
        const std::vector<Hub>& hubs,
        const std::unordered_map<HubId, std::vector<Channel>>& channels_by_hub,
        const nlohmann::json& online_by_hub, const UserId& current_user) const;
    nlohmann::json collect_online_members(const std::vector<Hub>& hubs) const;

    app::services::AuthService auth_service_;
    ChatDB& db_;
    net::ClientGateway& gateway_;
    net::ConnectionManager& connections_;
    app::services::HubPublisher& hub_publisher_;
};

}  // namespace app

#endif  // APP_COMMANDS_AUTHCOMMAND_H
