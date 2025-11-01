#include "app/Dispatcher.h"

namespace app {

void Dispatcher::register_cmd(std::string type, std::unique_ptr<ICommand> cmd) {
    map_[std::move(type)] = std::move(cmd);
}

std::optional<nlohmann::json> Dispatcher::dispatch(const std::string& type,
                                                   const CommandContext& ctx,
                                                   const nlohmann::json& j) {
    auto it = map_.find(type);
    if (it == map_.end()) {
        return nlohmann::json{{"type", "error"}, {"code", "unknown_type"}};
    }
    auto out = it->second->execute(ctx, j);

    // Convention: if command returns {"type":"auth_response","success":true,"user_id":...}
    if (set_auth_ && out.value("type", "") == "auth_response" && out.value("success", false)) {
        if (auto uid_it = out.find("user_id"); uid_it != out.end() && uid_it->is_string()) {
            set_auth_(ctx.conn_id, uid_it->get<UserId>());
        }
    }
    return out;
}

}  // namespace app
