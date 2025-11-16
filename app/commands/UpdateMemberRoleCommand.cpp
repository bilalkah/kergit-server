#include "app/commands/UpdateMemberRoleCommand.h"

#include "app/services/HubPublisher.h"
#include "app/services/PublicIdService.h"
#include "infra/persistence/PersistenceGateway.h"
#include "net/ClientGateway.h"
#include "net/ConnectionManager.h"
#include "net/PerSocketData.h"

#include <nlohmann/json.hpp>

#include <unordered_set>

using nlohmann::json;

namespace app {

UpdateMemberRoleCommand::UpdateMemberRoleCommand(PersistenceGateway& db, net::ClientGateway& gateway,
                                                 net::ConnectionManager& connections,
                                                 app::services::PublicIdService& ids,
                                                 app::services::HubPublisher& hub_publisher)
    : db_(db),
      gateway_(gateway),
      connections_(connections),
      ids_(ids),
      hub_publisher_(hub_publisher) {}

bool UpdateMemberRoleCommand::is_owner(net::PerSocketData& psd, const HubId& hub_id) {
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
    return role == Role::OWNER;
}

void UpdateMemberRoleCommand::execute(CommandContext& ctx) {
    auto& psd = ctx.psd;
    auto& output = ctx.output;
    const auto& data = ctx.input.data;

    if (!psd.authenticated) {
        output.success = false;
        output.error_code = "not_authenticated";
        output.error_message = "Authenticate before updating roles.";
        output.data = {{"type", "error"},
                       {"code", "not_authenticated"},
                       {"message", "Authentication required"}};
        return;
    }

    const std::string hub_id_str = data.value("hub_id", "");
    const std::string user_id_str = data.value("user_id", "");
    const std::string role_str = data.value("role", "");

    if (hub_id_str.empty() || user_id_str.empty() || role_str.empty()) {
        output.success = false;
        output.error_code = "invalid_request";
        output.error_message = "hub_id, user_id and role are required.";
        output.data = {{"type", "error"},
                       {"code", "invalid_request"},
                       {"message", "hub_id, user_id and role are required"}};
        return;
    }

    auto internal_hub = ids_.to_internal(PublicHubId{hub_id_str});
    auto internal_user = ids_.to_internal(PublicUserId{user_id_str});
    if (!internal_hub.has_value() || !internal_user.has_value()) {
        output.success = false;
        output.error_code = "not_found";
        output.error_message = "Invalid hub or user identifier.";
        output.data = {{"type", "error"},
                       {"code", "not_found"},
                       {"message", "Invalid hub or user identifier"}};
        return;
    }

    if (!is_owner(psd, *internal_hub)) {
        output.success = false;
        output.error_code = "insufficient_privilege";
        output.error_message = "Only owners can update member roles.";
        output.data = {{"type", "error"},
                       {"code", "insufficient_privilege"},
                       {"message", "Only owners can update member roles"}};
        return;
    }

    if (psd.user_id == *internal_user) {
        output.success = false;
        output.error_code = "invalid_target";
        output.error_message = "Owners cannot change their own role.";
        output.data = {{"type", "error"},
                       {"code", "invalid_target"},
                       {"message", "Owners cannot change their own role"}};
        return;
    }

    auto current_role = db_.hubs().getMembershipRole(*internal_hub, *internal_user);
    if (!current_role.has_value()) {
        output.success = false;
        output.error_code = "not_member";
        output.error_message = "Target user is not a member of the hub.";
        output.data = {{"type", "error"},
                       {"code", "not_member"},
                       {"message", "Target user is not a member of the hub"}};
        return;
    }
    if (*current_role == Role::OWNER) {
        output.success = false;
        output.error_code = "invalid_target";
        output.error_message = "Cannot modify the owner's role.";
        output.data = {{"type", "error"},
                       {"code", "invalid_target"},
                       {"message", "Cannot modify the owner's role"}};
        return;
    }

    std::string new_role_str;
    Role new_role;
    if (role_str == "admin") {
        new_role_str = "admin";
        new_role = Role::ADMIN;
    } else if (role_str == "member") {
        new_role_str = "member";
        new_role = Role::USER;
    } else {
        output.success = false;
        output.error_code = "invalid_role";
        output.error_message = "Role must be 'admin' or 'member'.";
        output.data = {{"type", "error"},
                       {"code", "invalid_role"},
                       {"message", "Role must be 'admin' or 'member'"}};
        return;
    }

    try {
        db_.hubs().addMember(*internal_hub, *internal_user, new_role_str);

        connections_.for_each([&](UwsSocket* ws) {
            if (!ws) return;
            auto* other = ws->getUserData();
            if (!other || !other->authenticated) return;
            if (!(other->user_id == *internal_user)) return;
            other->hub_roles[*internal_hub] = new_role;
            json notice = {{"type", "member_role_updated"},
                           {"hub_id", hub_id_str},
                           {"role", new_role_str}};
            gateway_.send_now(other->conn_id, notice);
        });

        hub_publisher_.publish_hub(*internal_hub);

        output.success = true;
        output.error_code.clear();
        output.error_message.clear();
        output.data = {{"type", "member_role_updated"},
                       {"hub_id", hub_id_str},
                       {"user_id", user_id_str},
                       {"role", new_role_str}};
        output.sent_at = std::chrono::system_clock::now();
    } catch (const std::exception& ex) {
        output.success = false;
        output.error_code = "update_failed";
        output.error_message = ex.what();
        output.data = {{"type", "error"},
                       {"code", "update_failed"},
                       {"message", ex.what()}};
    }
}

}  // namespace app
