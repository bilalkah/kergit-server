#ifndef APP_COMMANDS_SENDMESSAGECOMMAND_H
#define APP_COMMANDS_SENDMESSAGECOMMAND_H

#include "app/commands/ICommand.h"

class PersistenceGateway;

namespace net {
class ClientGateway;
}  // namespace net

namespace app::services {
class PublicIdService;
}

namespace app {

class SendMessageCommand : public ICommand {
   public:
    SendMessageCommand(PersistenceGateway& db, net::ClientGateway& gateway,
                       app::services::PublicIdService& ids);
    void execute(CommandContext&) override;

   private:
    static std::string channel_topic(const ChannelId& channel_id);

    PersistenceGateway& db_;
    net::ClientGateway& gateway_;
    app::services::PublicIdService& ids_;
};

}  // namespace app

#endif  // APP_COMMANDS_SENDMESSAGECOMMAND_H
