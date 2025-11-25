#ifndef APP_DISPATCHER_H
#define APP_DISPATCHER_H

#include "app/commands/ICommand.h"
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

    /**
     * Dispatch a command by its type.
     * @param type Command type string
     * @param ctx Command context to be filled/used by the command
     */
    void dispatch(const std::string& type, CommandContext& ctx);
    std::unordered_set<std::string> registered_commands() const;

   private:
    std::unordered_map<std::string, std::unique_ptr<ICommand>> map_;
};

}  // namespace app

#endif  // APP_DISPATCHER_H
