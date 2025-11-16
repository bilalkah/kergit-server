#include "app/commands/GetHubInviteCommand.h"

#include "app/services/PublicIdService.h"
#include "infra/persistence/PersistenceGateway.h"
#include "net/PerSocketData.h"

#include <nlohmann/json.hpp>
#include <string>

using nlohmann::json;

namespace app {

GetHubInviteCommand::GetHubInviteCommand(PersistenceGateway& db,
                                         app::services::PublicIdService& ids)
    : db_(db), ids_(ids) {}

bool GetHubInviteCommand::has_privilege(net::PerSocketData& psd, const HubId& hub_id) {
    auto it = psd.hub_roles.find(hub_id);
    Role role = Role::USER;
    if (it != psd.hub_roles.end()) {
        role = it->second;
    } else {
        auto db_role = db_.hubs().getMembershipRole(hub_id, psd.user_id);
        if (db_role.has_value()) {
            role = *db_role;
            psd.hub_roles[hub_id] = role;
        }
    }
    return role == Role::OWNER || role == Role::ADMIN;
}

void GetHubInviteCommand::execute(CommandContext& ctx) {
    auto& psd = ctx.psd;
    auto& output = ctx.output;
    const auto& data = ctx.input.data;

    if (!psd.authenticated) {
        output.success = false;
        output.error_code = "not_authenticated";
        output.error_message = "Authentication required.";
        output.data = {{"type", "error"},
                       {"code", "not_authenticated"},
                       {"message", "Authentication required"}};
        return;
    }

    const std::string hub_id_str = data.value("hub_id", "");
    if (hub_id_str.empty()) {
        output.success = false;
        output.error_code = "missing_hub_id";
        output.error_message = "hub_id is required.";
        output.data = {
            {"type", "error"}, {"code", "missing_hub_id"}, {"message", "hub_id is required"}};
        return;
    }

    auto internal_hub = ids_.to_internal(PublicHubId{hub_id_str});
    if (!internal_hub.has_value()) {
        output.success = false;
        output.error_code = "hub_not_found";
        output.error_message = "Hub not found.";
        output.data = {
            {"type", "error"}, {"code", "hub_not_found"}, {"message", "Hub does not exist"}};
        return;
    }

    if (!psd.hub_memberships.count(*internal_hub)) {
        if (!db_.hubs().isHubMember(*internal_hub, psd.user_id)) {
            output.success = false;
            output.error_code = "not_in_hub";
            output.error_message = "Join the hub before requesting an invite.";
            output.data = {{"type", "error"},
                           {"code", "not_in_hub"},
                           {"message", "Join the hub before requesting an invite"}};
            return;
        }
        psd.hub_memberships.insert(*internal_hub);
    }

    if (!has_privilege(psd, *internal_hub)) {
        output.success = false;
        output.error_code = "insufficient_privilege";
        output.error_message = "Only owners or admins can generate invites.";
        output.data = {{"type", "error"},
                       {"code", "insufficient_privilege"},
                       {"message", "Only owners or admins can generate invites"}};
        return;
    }

    const auto public_hub_id = ids_.to_public(*internal_hub);

    output.success = true;
    output.error_code.clear();
    output.error_message.clear();
    output.data = {{"type", "hub_invite"},
                   {"hub_id", public_hub_id.value},
                   {"invite_code", public_hub_id.value}};
    output.sent_at = std::chrono::system_clock::now();
}

}  // namespace app
