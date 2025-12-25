#ifndef APP_COMMANDS_DELETEHUBCOMMAND_H
#define APP_COMMANDS_DELETEHUBCOMMAND_H

#include "app/commands/ICommand.h"

namespace app {

class DeleteHubCommand : public ICommand {
   public:
    DeleteHubCommand(ServiceObjects& svc_objs);
    void execute(CommandContext&) override;

   private:
    bool is_owner(const CommandContext& ctx, const HubId& hub_id);

    std::string channel_topic(const ChannelId& channel_id) {
    return "channel:" + channel_id.value;
}

    ServiceObjects& services_;
};

}  // namespace app

#endif  // APP_COMMANDS_DELETEHUBCOMMAND_H
