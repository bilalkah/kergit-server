#include "app/Dispatcher.h"

namespace app {

void Dispatcher::register_cmd(std::string type, std::unique_ptr<ICommand> cmd) {
    map_[std::move(type)] = std::move(cmd);
}

std::optional<nlohmann::json> Dispatcher::dispatch(const std::string& type, CommandContext& ctx) {
    auto it = map_.find(type);
    if (it == map_.end()) {
        return nlohmann::json{{"type", "error"}, {"code", "unknown_type"}};
    }
    it->second->execute(ctx);
    auto out = ctx.output.data;

    // Convention: if command returns {"type":"auth_response","success":true,"user_id":...}
    if (set_auth_ && out.value("type", "") == "auth_response" &&
        out.value("success", false) == true) {
        if (auto uid_it = out.find("user_id"); uid_it != out.end() && uid_it->is_string()) {
            UserId user_id{uid_it->get<std::string>()};
            set_auth_(ctx.psd.conn_id, user_id);
        }
    }
    return out;
}

std::unordered_set<std::string> Dispatcher::registered_commands() const {
    std::unordered_set<std::string> commands;
    for (const auto& pair : map_) {
        commands.insert(pair.first);
    }
    return commands;
}

}  // namespace app
