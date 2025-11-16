#ifndef APP_COMMANDS_LEAVE_HUB_COMMAND_H
#define APP_COMMANDS_LEAVE_HUB_COMMAND_H

#include "app/commands/ICommand.h"
#include "app/services/HubPublisher.h"
#include "app/services/PublicIdService.h"
#include "infra/persistence/PersistenceGateway.h"
#include "net/ClientGateway.h"
#include "net/ConnectionManager.h"

#include <string>

namespace app {

class LeaveHubCommand : public ICommand {
   public:
    LeaveHubCommand(PersistenceGateway& db, net::ClientGateway& gateway,
                    net::ConnectionManager& connections, app::services::HubPublisher& hub_publisher,
                    app::services::PublicIdService& ids);
    void execute(CommandContext& ctx) override;

   private:
    bool is_owner(net::PerSocketData& psd, const HubId& hub_id);
    void publish_presence_update(const ChannelId& channel_id, const net::PerSocketData& psd,
                                 bool online);
    static std::string channel_topic(const ChannelId& channel_id);

    PersistenceGateway& db_;
    net::ClientGateway& gateway_;
    net::ConnectionManager& connections_;
    app::services::HubPublisher& hub_publisher_;
    app::services::PublicIdService& ids_;
};

}  // namespace app

#endif  // APP_COMMANDS_LEAVE_HUB_COMMAND_H
