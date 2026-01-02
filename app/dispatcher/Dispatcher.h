#ifndef APP_DISPATCHER_H
#define APP_DISPATCHER_H

#include "app/commands/ICommand.h"
#include "app/dispatcher/CommandContext.h"
#include "domains/ids/Ids.h"

#include <functional>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace app {

class Dispatcher {
   public:
    void register_cmd(std::string type, std::unique_ptr<ICommand> cmd);
    CommandResult dispatch(const std::string& type, CommandContext& ctx, const CommandInput cmd);
    std::unordered_set<std::string> registered_commands() const;
    void register_all();

   private:
    std::unordered_map<std::string, std::unique_ptr<ICommand>> map_;
};

}  // namespace app

#endif  // APP_DISPATCHER_H
