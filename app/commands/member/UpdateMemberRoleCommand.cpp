#include "app/commands/member/UpdateMemberRoleCommand.h"

#include "app/commands/CommandJson.h"
#include "app/dispatcher/CommandContext.h"
#include "domains/Hub.h"

#include <nlohmann/json.hpp>
#include <string>

using nlohmann::json;

namespace app {

namespace {
std::optional<Role> parse_role(const std::string& role_str) {
    if (role_str == "admin") return Role::ADMIN;
    if (role_str == "member") return Role::USER;
    return std::nullopt;
}
}  // namespace

CommandResult UpdateMemberRoleCommand::execute(CommandContext& ctx, const CommandInput cmd) {
    const auto* input = std::get_if<JsonInput>(&cmd);
    if (!input) {
        return std::unexpected(CommandError{1, "update_member_role expects JSON input"});
    }

    auto actor_exp = ctx.session_manager.sessionOfConnection(input->conn);
    if (!actor_exp.has_value()) {
        return std::unexpected(CommandError{2, "Authenticate first"});
    }
    const UserId actor = actor_exp.value();

    auto hub_raw = commands::read_uint64(input->body, "hub_id");
    auto user_raw = commands::read_uint64(input->body, "user_id");
    const auto j = json::parse(input->body, nullptr, false);
    if (j.is_discarded()) {
        return std::unexpected(CommandError{3, "Invalid JSON"});
    }

    const std::string role_raw = j.value("role", "");

    if (!hub_raw.has_value() || !user_raw.has_value() || role_raw.empty()) {
        return std::unexpected(
            CommandError{3, "hub_id, user_id and role are required"});
    }

    auto hub_id_opt = ctx.ids.to_internal(PublicHubId{hub_raw.value()});
    auto target_id_opt = ctx.ids.to_internal(PublicUserId{user_raw.value()});
    if (!hub_id_opt.has_value() || !target_id_opt.has_value()) {
        return std::unexpected(CommandError{4, "Invalid hub or user identifier"});
    }
    const HubId hub_id = hub_id_opt.value();
    const UserId target_id = target_id_opt.value();

    auto new_role_opt = parse_role(role_raw);
    if (!new_role_opt.has_value()) {
        return std::unexpected(CommandError{5, "Role must be 'admin' or 'member'"});
    }
    const Role new_role = *new_role_opt;

    // actor must be owner
    auto actor_role = ctx.hub_service.getMembershipRole(hub_id, actor);
    if (!actor_role.has_value()) {
        return std::unexpected(CommandError{6, "Join the hub before updating roles"});
    }
    if (*actor_role != Role::OWNER) {
        return std::unexpected(
            CommandError{7, "Only owners can update member roles"});
    }

    // target must be a member and not owner
    auto target_role = ctx.hub_service.getMembershipRole(hub_id, target_id);
    if (!target_role.has_value()) {
        return std::unexpected(CommandError{8, "Target user is not a member"});
    }
    if (*target_role == Role::OWNER) {
        return std::unexpected(CommandError{9, "Cannot modify the owner's role"});
    }

    if (actor == target_id) {
        return std::unexpected(CommandError{10, "Cannot change your own role"});
    }

    // Apply role change
    ctx.hub_service.addMember(hub_id, target_id, new_role);

    const auto public_hub_id = ctx.ids.to_public(hub_id);
    const auto public_target = ctx.ids.to_public(target_id);

    json payload = {{"type", "member_role_updated"},
                    {"hub_id", public_hub_id.value},
                    {"user_id", public_target.value},
                    {"role", role_raw}};

    CommandSuccess res;
    // Notify hub subscribers
    auto subs = ctx.subscription_manager.getSubscribers(Topic::HubTopic(hub_id));
    if (subs.has_value() && !subs->empty()) {
        std::vector<GlobalConnId> conns;
        conns.reserve(subs->size());
        for (const auto& uid : subs.value()) {
            auto c = ctx.session_manager.getMainConnection(uid);
            if (c.has_value()) conns.push_back(c.value());
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
