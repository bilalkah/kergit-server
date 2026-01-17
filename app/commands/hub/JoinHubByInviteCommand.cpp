#include "app/commands/hub/JoinHubByInviteCommand.h"

#include "app/commands/CommandJson.h"
#include "app/dispatcher/CommandContext.h"
#include "domains/Hub.h"

#include <nlohmann/json.hpp>
#include <string>
#include <unordered_set>

using nlohmann::json;

namespace app {

namespace {
json build_channels(CommandContext& ctx, const HubId& hub_id) {
    json channels_json = json::array();
    const auto channels = ctx.channel_service.getHubChannels(hub_id);
    for (const auto& channel : channels) {
        const auto public_channel_id = ctx.ids.to_public(channel.id);
        const auto public_hub_id = ctx.ids.to_public(channel.hub_id);
        std::string type = channel.type == ChannelType::VOICE ? "voice" : "text";
        channels_json.push_back({{"id", public_channel_id.value},
                                 {"hub_id", public_hub_id.value},
                                 {"name", channel.name},
                                 {"type", type}});
    }
    return channels_json;
}

json build_members(CommandContext& ctx, const HubId& hub_id, const UserId& self) {
    json members = json::array();
    const auto db_members = ctx.hub_service.getHubMembers(hub_id);
    const auto online_members = ctx.presence_manager.onlineUsersInHub(hub_id);
    std::unordered_set<UserId> online_set(online_members.begin(), online_members.end());

    for (const auto& [user_id, stored_display] : db_members) {
        const auto public_user = ctx.ids.to_public(user_id);
        std::string display = stored_display;
        if (display.empty()) {
            if (auto user = ctx.user_service.getUser(user_id)) {
                if (!user->username.empty())
                    display = user->username;
                else if (!user->full_name.empty())
                    display = user->full_name;
            }
        }
        if (display.empty()) display = "Member";
        const bool is_online = online_set.count(user_id) > 0 || user_id == self;
        members.push_back({{"handle", display},
                           {"display_name", display},
                           {"online", is_online},
                           {"user_id", public_user.value}});
    }
    return members;
}
}  // namespace

CommandResult JoinHubByInviteCommand::execute(CommandContext& ctx, const CommandInput cmd) {
    const auto* input = std::get_if<JsonInput>(&cmd);
    if (!input) {
        return std::unexpected(CommandError{1, "join_hub_by_code expects JSON input"});
    }

    auto user_exp = ctx.session_manager.sessionOfConnection(input->conn);
    if (!user_exp.has_value()) {
        return std::unexpected(CommandError{2, "Authenticate first"});
    }
    const UserId user_id = user_exp.value();

    auto invite_code = commands::read_uint64(input->body, "invite_code");
    if (!invite_code.has_value()) {
        return std::unexpected(CommandError{3, "Invite code is required"});
    }

    auto hub_id_opt = ctx.ids.to_internal(PublicHubId{invite_code.value()});
    if (!hub_id_opt.has_value()) {
        return std::unexpected(CommandError{4, "Invite code is invalid"});
    }
    const HubId hub_id = hub_id_opt.value();

    auto hub_opt = ctx.hub_service.getHub(hub_id);
    if (!hub_opt.has_value()) {
        return std::unexpected(CommandError{5, "Hub not found"});
    }

    // If already a member, just return snapshot data
    const bool already_member = ctx.hub_service.isHubMember(hub_id, user_id);
    if (!already_member) {
        ctx.hub_service.addMember(hub_id, user_id, Role::USER);
    }

    const auto public_hub_id = ctx.ids.to_public(hub_id);
    json hub_json = {{"id", public_hub_id.value}, {"name", hub_opt->name}, {"role", "member"}};
    json payload = {{"type", "hub_joined"},
                    {"hub", std::move(hub_json)},
                    {"channels", build_channels(ctx, hub_id)},
                    {"members", build_members(ctx, hub_id, user_id)},
                    {"already_member", already_member}};

    CommandSuccess res;
    res.intents.push_back(Unicast{.conn = input->conn, .payload = std::move(payload)});

    // Subscribe user to hub topic for future broadcasts
    ctx.subscription_manager.subscribe(user_id, Topic::HubTopic(hub_id));

    // Notify existing online members about the new online user
    const auto online_members = ctx.presence_manager.onlineUsersInHub(hub_id);
    if (!online_members.empty()) {
        std::vector<GlobalConnId> conns;
        conns.reserve(online_members.size());
        for (const auto& member_id : online_members) {
            if (member_id == user_id) continue;
            auto conn = ctx.session_manager.getMainConnection(member_id);
            if (conn.has_value()) conns.push_back(conn.value());
        }
        if (!conns.empty()) {
            auto payload_online = ctx.hub_notifier.memberOnline(hub_id, user_id);
            if (auto u = ctx.user_service.getUser(user_id)) {
                if (!u->username.empty())
                    payload_online["display_name"] = u->username;
                else if (!u->full_name.empty())
                    payload_online["display_name"] = u->full_name;
            }
            res.intents.push_back(
                Fanout{.conns = std::move(conns), .payload = std::move(payload_online)});
        }
    }

    return res;
}

}  // namespace app
