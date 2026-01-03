#include "app/commands/system/DisconnectionCommand.h"

#include <nlohmann/json.hpp>
#include <variant>
#include <vector>

using nlohmann::json;

namespace app {

CommandResult DisconnectionCommand::execute(CommandContext& ctx, const CommandInput cmd) {
    const auto* input = std::get_if<DisconnectEvent>(&cmd);
    if (!input) {
        return std::unexpected(
            CommandError{"invalid_input", "Disconnection command expects a disconnect event"});
    }

    auto session_exp = ctx.session_manager.sessionOfConnection(input->conn);
    if (!session_exp.has_value()) {
        return CommandSuccess{};
    }

    const UserId user_id = session_exp.value();
    const bool session_removed = ctx.session_manager.removeConnection(input->conn);

    CommandSuccess res;

    if (!session_removed) {
        return res;  // voice-only disconnect or still active elsewhere
    }

    ctx.subscription_manager.removeAllForUser(user_id);

    const auto hubs = ctx.hub_service.getUserHubs(user_id);
    for (const auto& hub : hubs) {
        const auto online_members = ctx.presence_manager.onlineUsersInHub(hub.id);
        std::vector<GlobalConnId> recipients;
        recipients.reserve(online_members.size());

        for (const auto& member_id : online_members) {
            if (member_id == user_id) continue;
            auto conn_exp = ctx.session_manager.getMainConnection(member_id);
            if (!conn_exp.has_value()) continue;
            recipients.push_back(conn_exp.value());
        }

        if (!recipients.empty()) {
            auto payload = ctx.hub_notifier.memberOffline(hub.id, user_id);
            if (auto user = ctx.user_service.getUser(user_id)) {
                if (!user->username.empty()) payload["display_name"] = user->username;
            }
            res.intents.push_back(Fanout{.conns = std::move(recipients),
                                         .payload = std::move(payload)});
        }
    }

    return res;
}

}  // namespace app
