#include "app/commands/CreateChannelCommand.h"

#include "app/services/HubPublisher.h"
#include "app/services/PublicIdService.h"
#include "infra/persistence/PersistenceGateway.h"
#include "net/PerSocketData.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <nlohmann/json.hpp>
#include <stdexcept>

using nlohmann::json;

namespace app {

CreateChannelCommand::CreateChannelCommand(PersistenceGateway& db, app::services::HubPublisher& hub_publisher,
                                           app::services::PublicIdService& ids)
    : db_(db), hub_publisher_(hub_publisher), ids_(ids) {}

bool CreateChannelCommand::has_privilege(net::PerSocketData& psd, const HubId& hub_id) {
    auto it = psd.hub_roles.find(hub_id);
    if (it != psd.hub_roles.end()) {
        const auto role = it->second;
        return role == Role::OWNER || role == Role::ADMIN;
    }
    auto role = db_.hubs().getMembershipRole(hub_id, psd.user_id);
    if (!role.has_value()) return false;
    psd.hub_roles[hub_id] = *role;
    return *role == Role::OWNER || *role == Role::ADMIN;
}

void CreateChannelCommand::execute(CommandContext& ctx) {
    auto& psd = ctx.psd;
    auto& output = ctx.output;
    const auto& data = ctx.input.data;

    if (!psd.authenticated) {
        output.success = false;
        output.error_code = "not_authenticated";
        output.error_message = "Authentication required";
        output.data = {{"type", "error"},
                       {"code", "not_authenticated"},
                       {"message", "Authentication required"}};
        return;
    }

    const std::string hub_id_str = data.value("hub_id", "");
    const std::string name_raw = data.value("name", "");
    const std::string type = data.value("type", "text");

    if (hub_id_str.empty()) {
        output.success = false;
        output.error_code = "missing_hub_id";
        output.error_message = "hub_id is required";
        output.data = {
            {"type", "error"}, {"code", "missing_hub_id"}, {"message", "hub_id is required"}};
        return;
    }

    auto hub_id_opt = ids_.to_internal(PublicHubId{hub_id_str});
    if (!hub_id_opt.has_value()) {
        output.success = false;
        output.error_code = "not_in_hub";
        output.error_message = "Join the hub before creating channels";
        output.data = {{"type", "error"},
                       {"code", "not_in_hub"},
                       {"message", "Join the hub before creating channels"}};
        return;
    }

    HubId hub_id = *hub_id_opt;
    if (!psd.hub_memberships.count(hub_id)) {
        output.success = false;
        output.error_code = "not_in_hub";
        output.error_message = "Join the hub before creating channels";
        output.data = {{"type", "error"},
                       {"code", "not_in_hub"},
                       {"message", "Join the hub before creating channels"}};
        return;
    }

    if (!has_privilege(psd, hub_id)) {
        output.success = false;
        output.error_code = "insufficient_privilege";
        output.error_message = "Only owners or admins can create channels";
        output.data = {{"type", "error"},
                       {"code", "insufficient_privilege"},
                       {"message", "Only owners or admins can create channels"}};
        return;
    }

    std::string name = name_raw;
    auto trim = [](std::string& s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                                        [](unsigned char ch) { return !std::isspace(ch); }));
        s.erase(
            std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); })
                .base(),
            s.end());
    };
    trim(name);

    if (name.empty()) {
        output.success = false;
        output.error_code = "invalid_name";
        output.error_message = "Channel name is required";
        output.data = {
            {"type", "error"}, {"code", "invalid_name"}, {"message", "Channel name is required"}};
        return;
    }

    if (name.size() > 48) name.resize(48);

    std::string channel_type = type == "voice" ? "voice" : "text";

    try {
        ChannelId created = db_.channels().createChannel(hub_id, name, channel_type);
        const auto public_hub_id = ids_.to_public(hub_id);
        const auto public_channel_id = ids_.to_public(created);

        nlohmann::json channel_json = {{"id", public_channel_id.value},
                                       {"hub_id", public_hub_id.value},
                                       {"name", name},
                                       {"type", channel_type}};

        output.success = true;
        output.error_code.clear();
        output.error_message.clear();
        output.data = {{"type", "channel_created"},
                       {"hub_id", public_hub_id.value},
                       {"channel", channel_json}};
        output.sent_at = std::chrono::system_clock::now();

        hub_publisher_.publish_hub(hub_id);
    } catch (const std::exception& ex) {
        output.success = false;
        output.error_code = "create_failed";
        output.error_message = ex.what();
        output.data = {{"type", "error"}, {"code", "create_failed"}, {"message", ex.what()}};
    }
}

}  // namespace app
