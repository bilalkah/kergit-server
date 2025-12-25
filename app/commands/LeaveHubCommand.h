#ifndef APP_COMMANDS_LEAVE_HUB_COMMAND_H
#define APP_COMMANDS_LEAVE_HUB_COMMAND_H

#include "app/commands/ICommand.h"

#include <string>

namespace app {

class LeaveHubCommand : public ICommand {
   public:
    LeaveHubCommand(ServiceObjects& svc_objs);
    void execute(CommandContext& ctx) override;

   private:
    bool is_owner(const CommandContext& ctx, const HubId& hub_id);
    void publish_presence_update(const ChannelId& channel_id, CommandContext& ctx,
                                 bool online);
    static std::string channel_topic(const ChannelId& channel_id);

    ServiceObjects& services_;
};

}  // namespace app

#endif  // APP_COMMANDS_LEAVE_HUB_COMMAND_H
