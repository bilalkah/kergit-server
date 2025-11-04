#ifndef APP_COMMANDS_CHANNELUSERSCOMMAND_H
#define APP_COMMANDS_CHANNELUSERSCOMMAND_H

#include "app/commands/ICommand.h"

#include <nlohmann/json.hpp>

namespace net {
class ConnectionManager;
}

namespace app {

class ChannelUsersCommand : public ICommand {
   public:
    explicit ChannelUsersCommand(net::ConnectionManager& connections);
    void execute(CommandContext&) override;

   private:
    nlohmann::json collect_channel_presence(const ChannelId& channel_id) const;

    net::ConnectionManager& connections_;
};

}  // namespace app

#endif  // APP_COMMANDS_CHANNELUSERSCOMMAND_H
