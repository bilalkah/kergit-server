#ifndef APP_COMMANDS_DELETECHANNELCOMMAND_H
#define APP_COMMANDS_DELETECHANNELCOMMAND_H

#include "app/commands/ICommand.h"

#include "domains/Channel.h"

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

class DeleteChannelCommand : public ICommand {
   public:
    DeleteChannelCommand(PersistenceGateway& db, net::ClientGateway& gateway, net::ConnectionManager& connections,
                         app::services::HubPublisher& hub_publisher,
                         app::services::PublicIdService& ids);
    void execute(CommandContext&) override;

   private:
    static std::string channel_topic(const ChannelId& channel_id);
    bool has_privilege(net::PerSocketData& psd, const HubId& hub_id);

    PersistenceGateway& db_;
    net::ClientGateway& gateway_;
    net::ConnectionManager& connections_;
    app::services::HubPublisher& hub_publisher_;
    app::services::PublicIdService& ids_;
};

}  // namespace app

#endif  // APP_COMMANDS_DELETECHANNELCOMMAND_H
