#include "app/commands/UpdateMemberRoleCommand.h"

#include "net/PerSocketData.h"

#include <nlohmann/json.hpp>
#include <unordered_set>

using nlohmann::json;

namespace app {

UpdateMemberRoleCommand::UpdateMemberRoleCommand(ServiceObjects& svc_objs)
    : services_(svc_objs) {}

bool UpdateMemberRoleCommand::is_owner(const CommandContext& ctx, const HubId& hub_id) {
    auto it = ctx.snapshot.roles.find(hub_id);
    if (it == ctx.snapshot.roles.end()) {
        return false;
    }
    Role role = it->second;
    return role == Role::OWNER;
}

void UpdateMemberRoleCommand::execute(CommandContext& ctx) {
    const auto& input = ctx.input;
    auto& output = ctx.output;

    if (!ctx.snapshot.authenticated) {
        output.success = false;
        output.error_code = "not_authenticated";
        output.error_message = "Authenticate before updating roles.";
        json err = {{"type", "error"},
                    {"code", "not_authenticated"},
                    {"message", "Authentication required"}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        output.messages.push_back(msg);
        output.sent_at = std::chrono::system_clock::now();
        return;
    }

    const std::string hub_id_str = input.data.value("hub_id", "");
    const std::string user_id_str = input.data.value("user_id", "");
    const std::string role_str = input.data.value("role", "");

    if (hub_id_str.empty() || user_id_str.empty() || role_str.empty()) {
        output.success = false;
        output.error_code = "invalid_request";
        output.error_message = "hub_id, user_id and role are required.";
        json err = {{"type", "error"},
                    {"code", "invalid_request"},
                    {"message", "hub_id, user_id and role are required"}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        output.messages.push_back(msg);
        output.sent_at = std::chrono::system_clock::now();
        return;
    }

    auto internal_hub = services_.ids_.to_internal(PublicHubId{hub_id_str});
    auto internal_user = services_.ids_.to_internal(PublicUserId{user_id_str});
    if (!internal_hub.has_value() || !internal_user.has_value()) {
        output.success = false;
        output.error_code = "not_found";
        output.error_message = "Invalid hub or user identifier.";
        json err = {{"type", "error"},
                    {"code", "not_found"},
                    {"message", "Invalid hub or user identifier"}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        output.messages.push_back(msg);
        output.sent_at = std::chrono::system_clock::now();
        return;
    }

    // if (!is_owner(ctx, *internal_hub)) {
    //     output.success = false;
    //     output.error_code = "insufficient_privilege";
    //     output.error_message = "Only owners can update member roles.";
    //     json err = {{"type", "error"},
    //                 {"code", "insufficient_privilege"},
    //                 {"message", "Only owners can update member roles"}};
    //     DirectMessage msg;
    //     msg.conn_id = ctx.conn_id;
    //     msg.payload = err.dump();
    //     output.messages.push_back(msg);
    //     output.sent_at = std::chrono::system_clock::now();
    //     return;
    // }

    if (ctx.snapshot.user_id == *internal_user) {
        output.success = false;
        output.error_code = "invalid_target";
        output.error_message = "Owners cannot change their own role.";
        json err = {{"type", "error"},
                    {"code", "invalid_target"},
                    {"message", "Owners cannot change their own role"}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        output.messages.push_back(msg);
        output.sent_at = std::chrono::system_clock::now();
        return;
    }

    auto current_role = services_.db_.hubs().getMembershipRole(*internal_hub, *internal_user);
    if (!current_role.has_value()) {
        output.success = false;
        output.error_code = "not_member";
        output.error_message = "Target user is not a member of the hub.";
        json err = {{"type", "error"},
                    {"code", "not_member"},
                    {"message", "Target user is not a member of the hub"}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        output.messages.push_back(msg);
        output.sent_at = std::chrono::system_clock::now();
        return;
    }
    if (*current_role == Role::OWNER) {
        output.success = false;
        output.error_code = "invalid_target";
        output.error_message = "Cannot modify the owner's role.";
        json err = {{"type", "error"},
                    {"code", "invalid_target"},
                    {"message", "Cannot modify the owner's role"}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        output.messages.push_back(msg);
        output.sent_at = std::chrono::system_clock::now();
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
        json err = {{"type", "error"},
                    {"code", "invalid_role"},
                    {"message", "Role must be 'admin' or 'member'"}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        output.messages.push_back(msg);
        output.sent_at = std::chrono::system_clock::now();
        return;
    }

    try {
        services_.db_.hubs().addMember(*internal_hub, *internal_user, new_role_str);

        services_.hub_publisher_.publish_hub(*internal_hub);

        json notice = {{"type", "member_role_updated"},
                       {"hub_id", hub_id_str},
                       {"user_id", user_id_str},
                       {"role", new_role_str}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = notice.dump();
        msg.apply_psd = [hub_id = *internal_hub, user_id = *internal_user,
                         new_role](net::PerSocketData* psd) {
            auto snapshot = *psd->snapshot;
            if (!snapshot.hubs.contains(hub_id)) {
                snapshot.hubs.insert(hub_id);
            }
            snapshot.roles[hub_id] = new_role;
            psd->snapshot = std::make_shared<const net::Snapshot>(std::move(snapshot));
        };
        output.messages.push_back(msg);
        output.success = true;
        output.error_code.clear();
        output.error_message.clear();
        output.sent_at = std::chrono::system_clock::now();
    } catch (const std::exception& ex) {
        output.success = false;
        output.error_code = "update_failed";
        output.error_message = ex.what();
        json err = {{"type", "error"}, {"code", "update_failed"}, {"message", ex.what()}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        output.messages.push_back(msg);
        output.sent_at = std::chrono::system_clock::now();
    }
}

}  // namespace app
