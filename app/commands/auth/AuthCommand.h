#ifndef APP_COMMANDS_AUTHCOMMAND_H
#define APP_COMMANDS_AUTHCOMMAND_H

#include "app/commands/ICommand.h"
#include "app/dispatcher/CommandContext.h"
#include "app/services/auth/AuthService.h"
#include "domains/Channel.h"
#include "domains/Hub.h"
#include "domains/ids/Ids.h"

#include <nlohmann/json.hpp>
#include <unordered_map>
#include <vector>

namespace app {

class AuthCommand : public ICommand {
   public:
    CommandResult execute(CommandContext& ctx, const CommandInput cmd) override;
};

}  // namespace app

#endif  // APP_COMMANDS_AUTHCOMMAND_H
