#ifndef APP_COMMANDS_DELETECHANNELCOMMAND_H
#define APP_COMMANDS_DELETECHANNELCOMMAND_H

#include "app/commands/ICommand.h"
#include "domains/Channel.h"

namespace app {

class DeleteChannelCommand : public ICommand {
   public:
    DeleteChannelCommand(ServiceObjects& svc_objs);
    void execute(CommandContext&) override;

   private:
    static std::string channel_topic(const ChannelId& channel_id);
    bool has_privilege(const CommandContext& ctx, const HubId& hub_id);

    ServiceObjects& services_;
};

}  // namespace app

#endif  // APP_COMMANDS_DELETECHANNELCOMMAND_H
