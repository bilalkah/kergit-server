#include "app/commands/RenameHubCommand.h"

#include "app/services/HubPublisher.h"
#include "app/services/PublicIdService.h"
#include "infra/persistence/PersistenceGateway.h"
#include "net/ClientGateway.h"
#include "net/ConnectionManager.h"
#include "net/PerSocketData.h"

#include <algorithm>
#include <cctype>
#include <nlohmann/json.hpp>
#include <string>

using nlohmann::json;

namespace app {

RenameHubCommand::RenameHubCommand(PersistenceGateway& db, net::ClientGateway& gateway,
                                   net::ConnectionManager& connections,
                                   app::services::HubPublisher& hub_publisher,
                                   app::services::PublicIdService& ids)
    : db_(db),
      gateway_(gateway),
      connections_(connections),
      hub_publisher_(hub_publisher),
      ids_(ids) {}

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

bool RenameHubCommand::is_owner(net::PerSocketData& psd, const HubId& hub_id) {
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

void RenameHubCommand::execute(CommandContext& ctx) {
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
    std::string requested_name = data.value("name", std::string{});

    if (hub_id_str.empty()) {
        output.success = false;
        output.error_code = "missing_hub_id";
        output.error_message = "hub_id is required.";
        output.data = {
            {"type", "error"}, {"code", "missing_hub_id"}, {"message", "hub_id is required"}};
        return;
    }

    requested_name = sanitize(std::move(requested_name));
    if (requested_name.empty()) {
        output.success = false;
        output.error_code = "invalid_hub_name";
        output.error_message = "Hub name is required.";
        output.data = {
            {"type", "error"}, {"code", "invalid_hub_name"}, {"message", "Hub name is required"}};
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
            output.error_message = "Join the hub before renaming it.";
            output.data = {{"type", "error"},
                           {"code", "not_in_hub"},
                           {"message", "Join the hub before renaming it"}};
            return;
        }
        psd.hub_memberships.insert(*internal_hub);
    }

    if (!is_owner(psd, *internal_hub)) {
        output.success = false;
        output.error_code = "insufficient_privilege";
        output.error_message = "Only owners can rename hubs.";
        output.data = {{"type", "error"},
                       {"code", "insufficient_privilege"},
                       {"message", "Only owners can rename hubs"}};
        return;
    }

    try {
        if (!db_.hubs().renameHub(*internal_hub, requested_name)) {
            output.success = false;
            output.error_code = "rename_hub_failed";
            output.error_message = "Unable to rename hub.";
            output.data = {{"type", "error"},
                           {"code", "rename_hub_failed"},
                           {"message", "Unable to rename hub"}};
            return;
        }

        const auto public_hub_id = ids_.to_public(*internal_hub);
        hub_publisher_.publish_hub(*internal_hub);

        json event = {
            {"type", "hub_renamed"}, {"hub_id", public_hub_id.value}, {"name", requested_name}};

        connections_.for_each([&](UwsSocket* ws) {
            if (!ws) return;
            auto* other = ws->getUserData();
            if (!other || !other->authenticated) return;
            if (other->hub_memberships.find(*internal_hub) == other->hub_memberships.end()) return;
            if (other->conn_id.value == psd.conn_id.value) return;
            gateway_.send_now(other->conn_id, event);
        });

        output.success = true;
        output.error_code.clear();
        output.error_message.clear();
        output.data = event;
        output.sent_at = std::chrono::system_clock::now();
    } catch (const std::exception& ex) {
        output.success = false;
        output.error_code = "rename_hub_failed";
        output.error_message = ex.what();
        output.data = {{"type", "error"}, {"code", "rename_hub_failed"}, {"message", ex.what()}};
    }
}

}  // namespace app
