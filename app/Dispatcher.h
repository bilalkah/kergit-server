#ifndef APP_DISPATCHER_H
#define APP_DISPATCHER_H

#include "app/commands/ICommand.h"
#include "domains/ids/Ids.h"

#include <functional>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <unordered_map>

namespace app {

class Dispatcher {
   public:
    using SetAuthFn = std::function<void(const ConnId& conn_id, const UserId& user_id)>;

    void register_cmd(std::string type, std::unique_ptr<ICommand> cmd);
    void on_auth_success(SetAuthFn fn) { set_auth_ = std::move(fn); }
    std::optional<nlohmann::json> dispatch(const std::string& type, CommandContext& ctx);

   private:
    std::unordered_map<std::string, std::unique_ptr<ICommand>> map_;
    SetAuthFn set_auth_{};
};

}  // namespace app

#endif  // APP_DISPATCHER_H
