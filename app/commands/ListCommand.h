#ifndef APP_COMMANDS_LISTCOMMAND_H
#define APP_COMMANDS_LISTCOMMAND_H

#include "app/commands/ICommand.h"
#include "domains/Channel.h"
#include "domains/Hub.h"

#include <nlohmann/json.hpp>

#include <unordered_map>
#include <vector>

class ChatDB;

namespace net {
class ClientGateway;
class ConnectionManager;
}  // namespace net

namespace app::services {
class HubPublisher;
}

namespace app {

class ListCommand : public ICommand {
   public:
    ListCommand(ChatDB& db, net::ConnectionManager& connections,
                app::services::HubPublisher& hub_publisher);
    void execute(CommandContext&) override;

   private:
    nlohmann::json build_payload(const std::vector<Hub>& hubs,
                                 const std::unordered_map<HubId, std::vector<Channel>>& channels,
                                 const nlohmann::json& online_by_hub, const UserId& current_user) const;
    nlohmann::json collect_online_members(const std::vector<Hub>& hubs) const;

    ChatDB& db_;
    net::ConnectionManager& connections_;
    app::services::HubPublisher& hub_publisher_;
};

}  // namespace app

#endif  // APP_COMMANDS_LISTCOMMAND_H
