#ifndef APP_COMMANDS_JOINCHANNELCOMMAND_H
#define APP_COMMANDS_JOINCHANNELCOMMAND_H

#include "app/commands/ICommand.h"
#include "domains/Channel.h"
#include "domains/Message.h"

#include <nlohmann/json.hpp>
#include <unordered_set>
#include <vector>

namespace app {

class JoinChannelCommand : public ICommand {
   public:
    JoinChannelCommand(ServiceObjects& svc_objs);
    void execute(CommandContext&) override;

   private:
    nlohmann::json collect_channel_presence(const ChannelId& channel_id, CommandContext& ctx) const;
    std::vector<Message> fetch_history(const ChannelId& channel_id);
    static std::string channel_topic(const ChannelId& channel_id);
    std::string resolve_display_name(const UserId& user_id) const;
    void publish_presence_update(const ChannelId& channel_id, CommandContext& ctx, bool online);

    ServiceObjects& services_;
};

}  // namespace app

#endif  // APP_COMMANDS_JOINCHANNELCOMMAND_H
