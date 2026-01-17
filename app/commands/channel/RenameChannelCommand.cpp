#include "app/commands/channel/RenameChannelCommand.h"

#include "app/commands/CommandJson.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "domains/Channel.h"
#include "domains/Hub.h"

#include <algorithm>
#include <cctype>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using nlohmann::json;

namespace app {

namespace {
std::string channel_type_to_string(ChannelType type) {
    return type == ChannelType::VOICE ? "voice" : "text";
}
}  // namespace

std::string RenameChannelCommand::sanitize(std::string name) {
    auto trim = [](std::string& s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                                        [](unsigned char ch) { return !std::isspace(ch); }));
        s.erase(
            std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); })
                .base(),
            s.end());
    };
    trim(name);
    if (name.size() > 48) name.resize(48);
    return name;
}

CommandResult RenameChannelCommand::execute(CommandContext& ctx, const CommandInput cmd) {
    const auto* input = std::get_if<JsonInput>(&cmd);
    if (!input) {
        return std::unexpected(CommandError{1, "rename_channel expects JSON input"});
    }

    auto user_exp = ctx.session_manager.sessionOfConnection(input->conn);
    if (!user_exp.has_value()) {
        return std::unexpected(CommandError{2, "Authenticate first"});
    }
    const UserId user_id = user_exp.value();

    auto channel_id_raw = commands::read_uint64(input->body, "channel_id");
    const auto j = json::parse(input->body, nullptr, false);
    if (j.is_discarded()) {
        return std::unexpected(CommandError{3, "Invalid JSON"});
    }
    std::string requested_name = sanitize(j.value("name", ""));

    if (!channel_id_raw.has_value()) {
        return std::unexpected(CommandError{3, "channel_id is required"});
    }
    if (requested_name.empty()) {
        return std::unexpected(CommandError{4, "Channel name is required"});
    }

    auto channel_id_opt = ctx.ids.to_internal(PublicChannelId{channel_id_raw.value()});
    if (!channel_id_opt.has_value()) {
        return std::unexpected(CommandError{5, "Channel not found"});
    }

    auto channel_opt = ctx.channel_service.getChannel(*channel_id_opt);
    if (!channel_opt.has_value()) {
        return std::unexpected(CommandError{6, "Channel not found"});
    }
    const Channel channel = channel_opt.value();
    const HubId hub_id = channel.hub_id;

    if (!ctx.hub_service.isHubMember(hub_id, user_id)) {
        return std::unexpected(CommandError{7, "Join the hub before renaming channels"});
    }

    auto role = ctx.hub_service.getMembershipRole(hub_id, user_id);
    if (!role.has_value() || (*role != Role::OWNER && *role != Role::ADMIN)) {
        return std::unexpected(CommandError{8, "Only admins/owners can rename channels"});
    }

    if (!ctx.channel_service.renameChannel(channel.id, requested_name)) {
        return std::unexpected(CommandError{9, "Unable to rename channel at this time"});
    }

    const auto public_hub_id = ctx.ids.to_public(hub_id);
    const auto public_channel_id = ctx.ids.to_public(channel.id);

    json channel_json = {{"id", public_channel_id.value},
                         {"hub_id", public_hub_id.value},
                         {"name", requested_name},
                         {"type", channel_type_to_string(channel.type)}};

    json payload = {
        {"type", "channel_renamed"}, {"hub_id", public_hub_id.value}, {"channel", channel_json}};

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
