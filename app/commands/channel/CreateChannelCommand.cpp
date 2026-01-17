#include "app/commands/channel/CreateChannelCommand.h"

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
        return std::unexpected(CommandError{1, "create_channel expects JSON input"});
    }

    auto user_exp = ctx.session_manager.sessionOfConnection(input->conn);
    if (!user_exp.has_value()) {
        return std::unexpected(CommandError{2, "Authenticate first"});
    }
    const UserId user_id = user_exp.value();

    const auto& j = json::parse(input->body, nullptr, false);
    if (j.is_discarded()) {
        return std::unexpected(CommandError{1, "Invalid JSON"});
    }
    auto hub_raw = commands::read_uint64(j, "hub_id");
    std::string name = sanitize_name(j.value("name", ""));
    const std::string type = j.value("type", "text");

    if (!hub_raw.has_value()) {
        return std::unexpected(CommandError{3, "hub_id is required"});
    }
    if (name.empty()) {
        return std::unexpected(CommandError{4, "Channel name is required"});
    }

    auto hub_id_opt = ctx.ids.to_internal(PublicHubId{hub_raw.value()});
    if (!hub_id_opt.has_value()) {
        return std::unexpected(CommandError{5, "Hub not found"});
    }
    const HubId hub_id = hub_id_opt.value();

    if (!ctx.hub_service.isHubMember(hub_id, user_id)) {
        return std::unexpected(CommandError{6, "Join the hub before creating channels"});
    }

    auto role = ctx.hub_service.getMembershipRole(hub_id, user_id);
    if (!role.has_value() || (*role != Role::OWNER && *role != Role::ADMIN)) {
        return std::unexpected(CommandError{7, "Only admins/owners can create channels"});
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
    res.intents.reserve(2);  // fanout to hub + ack to creator at most
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
            res.intents.emplace_back(std::in_place_type<Fanout>, std::move(conns),
                                     json{{"type", "channel_created"},
                                          {"hub_id", public_hub_id.value},
                                          {"channel", channel_json}});
        }
    }

    // Ack creator
    res.intents.emplace_back(std::in_place_type<Unicast>, input->conn,
                             json{{"type", "channel_created"},
                                  {"hub_id", public_hub_id.value},
                                  {"channel", channel_json}});

    return res;
}

}  // namespace app
