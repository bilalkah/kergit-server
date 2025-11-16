#include "app/commands/RenameChannelCommand.h"

#include "app/services/HubPublisher.h"
#include "app/services/PublicIdService.h"
#include "infra/persistence/PersistenceGateway.h"
#include "net/ClientGateway.h"
#include "net/PerSocketData.h"

#include <algorithm>
#include <cctype>
#include <nlohmann/json.hpp>
#include <string>

using nlohmann::json;

namespace app {

namespace {
std::string channel_type_to_string(ChannelType type) {
    return type == ChannelType::VOICE ? "voice" : "text";
}
}  // namespace

RenameChannelCommand::RenameChannelCommand(PersistenceGateway& db, net::ClientGateway& gateway,
                                           app::services::HubPublisher& hub_publisher,
                                           app::services::PublicIdService& ids)
    : db_(db), gateway_(gateway), hub_publisher_(hub_publisher), ids_(ids) {}

std::string RenameChannelCommand::sanitize(std::string name) {
    auto trim = [](std::string& s) {
        s.erase(s.begin(),
                std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
        s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); })
                    .base(),
                s.end());
    };
    trim(name);
    if (name.size() > 48) name.resize(48);
    return name;
}

bool RenameChannelCommand::has_privilege(net::PerSocketData& psd, const HubId& hub_id) {
    auto it = psd.hub_roles.find(hub_id);
    if (it != psd.hub_roles.end()) {
        auto role = it->second;
        return role == Role::OWNER || role == Role::ADMIN;
    }
    auto role = db_.hubs().getMembershipRole(hub_id, psd.user_id);
    if (!role.has_value()) return false;
    psd.hub_roles[hub_id] = *role;
    return *role == Role::OWNER || *role == Role::ADMIN;
}

void RenameChannelCommand::execute(CommandContext& ctx) {
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

    const std::string channel_id_str = data.value("channel_id", "");
    std::string requested_name = data.value("name", std::string{});

    if (channel_id_str.empty()) {
        output.success = false;
        output.error_code = "missing_channel_id";
        output.error_message = "channel_id is required.";
        output.data = {{"type", "error"},
                       {"code", "missing_channel_id"},
                       {"message", "channel_id is required"}};
        return;
    }

    requested_name = sanitize(std::move(requested_name));
    if (requested_name.empty()) {
        output.success = false;
        output.error_code = "invalid_channel_name";
        output.error_message = "Channel name is required.";
        output.data = {{"type", "error"},
                       {"code", "invalid_channel_name"},
                       {"message", "Channel name is required"}};
        return;
    }

    auto internal_channel = ids_.to_internal(PublicChannelId{channel_id_str});
    if (!internal_channel.has_value()) {
        output.success = false;
        output.error_code = "channel_not_found";
        output.error_message = "Channel not found.";
        output.data = {{"type", "error"},
                       {"code", "channel_not_found"},
                       {"message", "Channel does not exist"}};
        return;
    }

    auto channel = db_.channels().getChannel(*internal_channel);
    if (!channel.has_value()) {
        output.success = false;
        output.error_code = "channel_not_found";
        output.error_message = "Channel not found.";
        output.data = {{"type", "error"},
                       {"code", "channel_not_found"},
                       {"message", "Channel does not exist"}};
        return;
    }

    if (!psd.hub_memberships.count(channel->hub_id)) {
        if (!db_.hubs().isHubMember(channel->hub_id, psd.user_id)) {
            output.success = false;
            output.error_code = "not_in_hub";
            output.error_message = "Join the hub before renaming channels.";
            output.data = {{"type", "error"},
                           {"code", "not_in_hub"},
                           {"message", "Join the hub before renaming channels"}};
            return;
        }
        psd.hub_memberships.insert(channel->hub_id);
    }

    if (!has_privilege(psd, channel->hub_id)) {
        output.success = false;
        output.error_code = "insufficient_privilege";
        output.error_message = "Only owners or admins can rename channels.";
        output.data = {{"type", "error"},
                       {"code", "insufficient_privilege"},
                       {"message", "Only owners or admins can rename channels"}};
        return;
    }

    try {
        if (!db_.channels().renameChannel(channel->channel_id, requested_name)) {
            output.success = false;
        output.error_code = "rename_channel_failed";
        output.error_message = "Unable to rename channel.";
        output.data = {{"type", "error"},
                       {"code", "rename_channel_failed"},
                       {"message", "Unable to rename channel"}};
            return;
        }

        const auto public_channel_id = ids_.to_public(channel->channel_id);
        const auto public_hub_id = ids_.to_public(channel->hub_id);

        nlohmann::json channel_json = {{"id", public_channel_id.value},
                                       {"hub_id", public_hub_id.value},
                                       {"name", requested_name},
                                       {"type", channel_type_to_string(channel->type)}};

        hub_publisher_.publish_hub(channel->hub_id);

        output.success = true;
        output.error_code.clear();
        output.error_message.clear();
        output.data = {{"type", "channel_renamed"},
                       {"hub_id", public_hub_id.value},
                       {"channel", channel_json}};
        output.sent_at = std::chrono::system_clock::now();
    } catch (const std::exception& ex) {
        output.success = false;
        output.error_code = "rename_channel_failed";
        output.error_message = ex.what();
        output.data = {
            {"type", "error"}, {"code", "rename_channel_failed"}, {"message", ex.what()}};
    }
}

}  // namespace app
