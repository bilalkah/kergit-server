#include "app/Dispatcher.h"

namespace app {

void Dispatcher::register_cmd(std::string type, std::unique_ptr<ICommand> cmd) {
    map_[std::move(type)] = std::move(cmd);
}

void Dispatcher::dispatch(const std::string& type, CommandContext& ctx) {
    auto it = map_.find(type);
    if (it == map_.end()) {
        ctx.output.success = false;
        ctx.output.error_code = "unknown_command";
        ctx.output.error_message = "The command type is not recognized";
        return;
    }

    it->second->execute(ctx);
}

std::unordered_set<std::string> Dispatcher::registered_commands() const {
    std::unordered_set<std::string> commands;
    for (const auto& pair : map_) {
        commands.insert(pair.first);
    }
    return commands;
}

}  // namespace app
