#ifndef APP_COMMANDS_SENDMESSAGECOMMAND_H
#define APP_COMMANDS_SENDMESSAGECOMMAND_H

#include "app/commands/ICommand.h"

namespace app {

class SendMessageCommand : public ICommand {
   public:
    SendMessageCommand(ServiceObjects& svc_objs);
    void execute(CommandContext&) override;

   private:
    static std::string channel_topic(const ChannelId& channel_id);

    ServiceObjects& services_;
};

}  // namespace app

#endif  // APP_COMMANDS_SENDMESSAGECOMMAND_H
