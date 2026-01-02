#include "app/commands/channel/CreateChannelCommand.h"

#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "domains/Channel.h"
#include "domains/Hub.h"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

using nlohmann::json;

namespace app {

namespace {
std::string sanitize_name(std::string name) {
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
}  // namespace

CommandResult CreateChannelCommand::execute(CommandContext& ctx, const CommandInput cmd) {
    const auto* input = std::get_if<JsonInput>(&cmd);
    if (!input) {
        return std::unexpected(CommandError{"invalid_input", "create_channel expects JSON input"});
    }

    auto user_exp = ctx.session_manager.sessionOfConnection(input->conn);
    if (!user_exp.has_value()) {
        return std::unexpected(CommandError{"not_authenticated", "Authenticate first"});
    }
    const UserId user_id = user_exp.value();

    const std::string hub_raw = input->body.value("hub_id", "");
    std::string name = sanitize_name(input->body.value("name", ""));
    const std::string type = input->body.value("type", "text");

    if (hub_raw.empty()) {
        return std::unexpected(CommandError{"missing_hub_id", "hub_id is required"});
    }
    if (name.empty()) {
        return std::unexpected(CommandError{"invalid_name", "Channel name is required"});
    }

    auto hub_id_opt = ctx.ids.to_internal(PublicHubId{hub_raw});
    if (!hub_id_opt.has_value()) {
        return std::unexpected(CommandError{"hub_not_found", "Hub not found"});
    }
    const HubId hub_id = hub_id_opt.value();

    if (!ctx.hub_service.isHubMember(hub_id, user_id)) {
        return std::unexpected(CommandError{"not_in_hub", "Join the hub before creating channels"});
    }

    auto role = ctx.hub_service.getMembershipRole(hub_id, user_id);
    if (!role.has_value() || (*role != Role::OWNER && *role != Role::ADMIN)) {
        return std::unexpected(CommandError{"insufficient_privilege",
                                            "Only admins/owners can create channels"});
    }

    std::string channel_type = type == "voice" ? "voice" : "text";
    ChannelId created = ctx.channel_service.createChannel(hub_id, name, channel_type);
    const auto public_hub_id = ctx.ids.to_public(hub_id);
    const auto public_channel_id = ctx.ids.to_public(created);

    json channel_json = {{"id", public_channel_id.value},
                         {"hub_id", public_hub_id.value},
                         {"name", name},
                         {"type", channel_type}};

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
            res.intents.push_back(Fanout{
                .conns = std::move(conns),
                .payload = json{{"type", "channel_created"},
                                {"hub_id", public_hub_id.value},
                                {"channel", channel_json}}});
        }
    }

    // Ack creator
    res.intents.push_back(
        Unicast{.conn = input->conn,
                .payload = json{{"type", "channel_created"},
                                {"hub_id", public_hub_id.value},
                                {"channel", channel_json}}});

    return res;
}

}  // namespace app
