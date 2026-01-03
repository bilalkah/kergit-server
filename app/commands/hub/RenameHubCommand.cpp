#include "app/commands/hub/RenameHubCommand.h"

#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "domains/Hub.h"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <string>

using nlohmann::json;

namespace app {

std::string RenameHubCommand::sanitize(std::string name) {
    auto trim = [](std::string& s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                                        [](unsigned char ch) { return !std::isspace(ch); }));
        s.erase(
            std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); })
                .base(),
            s.end());
    };
    trim(name);
    if (name.size() > 64) name.resize(64);
    return name;
}

CommandResult RenameHubCommand::execute(CommandContext& ctx, const CommandInput cmd) {
    const auto* input = std::get_if<JsonInput>(&cmd);
    if (!input) {
        return std::unexpected(CommandError{"invalid_input", "rename_hub expects JSON input"});
    }

    auto user_exp = ctx.session_manager.sessionOfConnection(input->conn);
    if (!user_exp.has_value()) {
        return std::unexpected(CommandError{"not_authenticated", "Authenticate first"});
    }
    const UserId user_id = user_exp.value();

    const std::string hub_raw = input->body.value("hub_id", "");
    std::string requested_name = sanitize(input->body.value("name", std::string{}));

    if (hub_raw.empty()) {
        return std::unexpected(CommandError{"missing_hub_id", "hub_id is required"});
    }
    if (requested_name.empty()) {
        return std::unexpected(CommandError{"invalid_hub_name", "Hub name is required"});
    }

    auto hub_id_opt = ctx.ids.to_internal(PublicHubId{hub_raw});
    if (!hub_id_opt.has_value()) {
        return std::unexpected(CommandError{"hub_not_found", "Hub not found"});
    }
    const HubId hub_id = hub_id_opt.value();

    if (!ctx.hub_service.isHubMember(hub_id, user_id)) {
        return std::unexpected(CommandError{"not_in_hub", "Join the hub before renaming it"});
    }

    auto role = ctx.hub_service.getMembershipRole(hub_id, user_id);
    if (!role.has_value() || *role != Role::OWNER) {
        return std::unexpected(CommandError{"insufficient_privilege", "Only owners can rename hubs"});
    }

    if (!ctx.hub_service.renameHub(hub_id, requested_name)) {
        return std::unexpected(CommandError{"rename_hub_failed", "Unable to rename hub at this time"});
    }

    const auto public_hub_id = ctx.ids.to_public(hub_id);
    json payload = {{"type", "hub_renamed"}, {"hub_id", public_hub_id.value}, {"name", requested_name}};

    CommandSuccess res;
    // Notify hub subscribers
    auto subs = ctx.subscription_manager.getSubscribers(Topic::HubTopic(hub_id));
    if (subs.has_value() && !subs->empty()) {
        std::vector<GlobalConnId> conns;
        conns.reserve(subs->size());
        for (const auto& uid : subs.value()) {
            auto conn = ctx.session_manager.getMainConnection(uid);
            if (conn.has_value()) conns.push_back(conn.value());
        }
        if (!conns.empty()) {
            res.intents.push_back(Fanout{.conns = std::move(conns), .payload = payload});
        }
    }

    // Ack requester
    res.intents.push_back(Unicast{.conn = input->conn, .payload = std::move(payload)});

    return res;
}

}  // namespace app
