#ifndef APP_COMMANDS_SENDMESSAGECOMMAND_H
#define APP_COMMANDS_SENDMESSAGECOMMAND_H

#include "app/commands/ICommand.h"

class ChatDB;

namespace net {
class ClientGateway;
}  // namespace net

namespace app {

class SendMessageCommand : public ICommand {
   public:
    SendMessageCommand(ChatDB& db, net::ClientGateway& gateway);
    void execute(CommandContext&) override;

   private:
    static std::string channel_topic(const ChannelId& channel_id);

    ChatDB& db_;
    net::ClientGateway& gateway_;
};

}  // namespace app

#endif  // APP_COMMANDS_SENDMESSAGECOMMAND_H
