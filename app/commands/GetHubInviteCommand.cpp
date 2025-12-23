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

bool GetHubInviteCommand::has_privilege(const CommandContext& ctx, const HubId& hub_id) {
    auto it = ctx.snapshot.roles.find(hub_id);
    if (it == ctx.snapshot.roles.end()) return false;

    Role role = it->second;
    return role == Role::OWNER || role == Role::ADMIN;
}

void GetHubInviteCommand::execute(CommandContext& ctx) {
    const auto& input = ctx.input;
    auto& output = ctx.output;

    if (!ctx.authenticated) {
        output.success = false;
        output.error_code = "not_authenticated";
        output.error_message = "Authentication required.";
        json err = {{"type", "error"},
                    {"code", "not_authenticated"},
                    {"message", "Authentication required"}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
        return;
    }

    const std::string hub_id_str = input.data.value("hub_id", "");
    if (hub_id_str.empty()) {
        output.success = false;
        output.error_code = "missing_hub_id";
        output.error_message = "hub_id is required.";
        json err = {
            {"type", "error"}, {"code", "missing_hub_id"}, {"message", "hub_id is required"}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
        return;
    }

    auto internal_hub = ids_.to_internal(PublicHubId{hub_id_str});
    if (!internal_hub.has_value()) {
        output.success = false;
        output.error_code = "hub_not_found";
        output.error_message = "Hub not found.";
        json err = {
            {"type", "error"}, {"code", "hub_not_found"}, {"message", "Hub does not exist"}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
        return;
    }

    if (!ctx.snapshot.hubs.count(*internal_hub)) {
        if (!db_.hubs().isHubMember(*internal_hub, ctx.user_id)) {
            output.success = false;
            output.error_code = "not_in_hub";
            output.error_message = "Join the hub before requesting an invite.";
            json err = {{"type", "error"},
                        {"code", "not_in_hub"},
                        {"message", "Join the hub before requesting an invite"}};
            DirectMessage msg;
            msg.conn_id = ctx.conn_id;
            msg.payload = err.dump();
            ctx.output.messages.push_back(std::move(msg));
            output.sent_at = std::chrono::system_clock::now();
            return;
        }
        ctx.snapshot.hubs.insert(*internal_hub);
    }

    if (!has_privilege(ctx, *internal_hub)) {
        output.success = false;
        output.error_code = "insufficient_privilege";
        output.error_message = "Only owners or admins can generate invites.";
        json err = {{"type", "error"},
                    {"code", "insufficient_privilege"},
                    {"message", "Only owners or admins can generate invites"}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
        return;
    }

    const auto public_hub_id = ids_.to_public(*internal_hub);

    output.success = true;
    output.error_code.clear();
    output.error_message.clear();
    json data = {{"type", "hub_invite"},
                 {"hub_id", public_hub_id.value},
                 {"invite_code", public_hub_id.value}};
    DirectMessage msg;
    msg.conn_id = ctx.conn_id;
    msg.payload = data.dump();
    ctx.output.messages.push_back(std::move(msg));
    output.sent_at = std::chrono::system_clock::now();
}

}  // namespace app
