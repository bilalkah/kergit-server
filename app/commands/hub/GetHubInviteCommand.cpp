#include "app/commands/hub/GetHubInviteCommand.h"

#include "app/commands/CommandJson.h"
#include "app/dispatcher/CommandContext.h"
#include "domains/Hub.h"

#include <nlohmann/json.hpp>
#include <string>

using nlohmann::json;

namespace app {

CommandResult GetHubInviteCommand::execute(CommandContext& ctx, const CommandInput cmd) {
    const auto* input = std::get_if<JsonInput>(&cmd);
    if (!input) {
        return std::unexpected(CommandError{"invalid_input", "generate_hub_invite expects JSON input"});
    }

    auto user_exp = ctx.session_manager.sessionOfConnection(input->conn);
    if (!user_exp.has_value()) {
        return std::unexpected(CommandError{"not_authenticated", "Authenticate first"});
    }
    const UserId user_id = user_exp.value();

    auto hub_raw = commands::read_uint64(input->body, "hub_id");
    if (!hub_raw.has_value()) {
        return std::unexpected(CommandError{"missing_hub_id", "hub_id is required"});
    }

    auto hub_id_opt = ctx.ids.to_internal(PublicHubId{hub_raw.value()});
    if (!hub_id_opt.has_value()) {
        return std::unexpected(CommandError{"hub_not_found", "Hub not found"});
    }
    const HubId hub_id = hub_id_opt.value();

    if (!ctx.hub_service.isHubMember(hub_id, user_id)) {
        return std::unexpected(CommandError{"not_in_hub", "Join the hub before requesting an invite"});
    }

    auto role = ctx.hub_service.getMembershipRole(hub_id, user_id);
    if (!role.has_value() || (*role != Role::OWNER && *role != Role::ADMIN)) {
        return std::unexpected(
            CommandError{"insufficient_privilege", "Only owners or admins can generate invites"});
    }

    const auto public_hub_id = ctx.ids.to_public(hub_id);
    json payload = {{"type", "hub_invite"},
                    {"hub_id", public_hub_id.value},
                    {"invite_code", public_hub_id.value}};

    CommandSuccess res;
    res.intents.push_back(Unicast{.conn = input->conn, .payload = std::move(payload)});
    return res;
}

}  // namespace app
