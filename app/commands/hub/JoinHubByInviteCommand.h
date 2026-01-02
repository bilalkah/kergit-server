#ifndef APP_COMMANDS_JOINHUBBYINVITECOMMAND_H
#define APP_COMMANDS_JOINHUBBYINVITECOMMAND_H

#include "app/commands/ICommand.h"

#include <nlohmann/json.hpp>

namespace app {

class JoinHubByInviteCommand : public ICommand {
   public:
    JoinHubByInviteCommand(ServiceObjects& svc_objs);
    void execute(CommandContext&) override;

   private:
    nlohmann::json build_members_payload(const HubId& hub_id);
    nlohmann::json build_channels_payload(const HubId& hub_id);

    ServiceObjects& services_;
};

}  // namespace app

#endif  // APP_COMMANDS_JOINHUBBYINVITECOMMAND_H
