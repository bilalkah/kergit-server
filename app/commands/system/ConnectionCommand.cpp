#include "app/commands/system/ConnectionCommand.h"

#include "app/managers/subscription/Topic.h"
#include "domains/Channel.h"
#include "domains/Hub.h"

#include <nlohmann/json.hpp>
#include <string>
#include <unordered_set>
#include <variant>
#include <vector>

using nlohmann::json;

namespace app {

std::vector<net::outbound::OutgoingMessage> ConnectionCommand::execute(CommandContext& ctx,
                                                                       const queue::Event& evt) {
    const auto* input = std::get_if<ConnectEvent>(&cmd);
    if (!input) {
        return std::unexpected(CommandError{1, "Connection command expects a connect event"});
    }

    const UserId user_id = input->user_id;
    if (user_id.value.empty()) {
        return std::unexpected(CommandError{2, "User id is required"});
    }

    auto db_user = ctx.user_service.getUser(user_id);
    if (!db_user) {
        return std::unexpected(CommandError{3, "User not found in database"});
    }

    const std::string display_name =
        !db_user->username.empty() ? db_user->username
                                   : (!db_user->full_name.empty() ? db_user->full_name : "Member");

    // Track session + subscribe to hubs for presence
    ctx.session_manager.createSession(input->conn, user_id);
    const auto hubs = ctx.hub_service.getUserHubs(user_id);
    for (const auto& hub : hubs) {
        ctx.subscription_manager.subscribe(user_id, Topic::HubTopic(hub.id));
    }

    const auto public_user_id = ctx.ids.to_public(user_id);
    auto hubs_meta = json::array();
    json channels_by_hub = json::object();
    json members_by_hub = json::object();

    for (const auto& hub : hubs) {
        const auto public_hub_id = ctx.ids.to_public(hub.id);
        std::string role = "member";
        if (auto it = hub.members.find(user_id); it != hub.members.end()) {
            switch (it->second) {
                case Role::ADMIN:
                    role = "admin";
                    break;
                case Role::OWNER:
                    role = "owner";
                    break;
                case Role::USER:
                default:
                    role = "member";
                    break;
            }
        }

        const auto hub_channels = ctx.channel_service.getHubChannels(hub.id);
        hubs_meta.push_back({{"id", public_hub_id.value},
                             {"name", hub.name},
                             {"role", role},
                             {"channels_count", hub_channels.size()}});

        json channels_json = json::array();
        for (const auto& channel : hub_channels) {
            const auto public_channel = ctx.ids.to_public(channel.id);
            const std::string type_str = channel.type == ChannelType::VOICE ? "voice" : "text";
            channels_json.push_back(
                {{"id", public_channel.value}, {"name", channel.name}, {"type", type_str}});
        }
        const auto hub_key = std::to_string(public_hub_id.value);
        channels_by_hub[hub_key] = std::move(channels_json);

        json members_json = json::array();
        const auto online_users = ctx.presence_manager.onlineUsersInHub(hub.id);
        std::unordered_set<UserId> online_set(online_users.begin(), online_users.end());
        const auto members = ctx.hub_service.getHubMembers(hub.id);
        for (const auto& [member_id, stored_display] : members) {
            const auto public_member = ctx.ids.to_public(member_id);
            const bool is_online = online_set.find(member_id) != online_set.end();

            std::string name = stored_display;
            if (name.empty()) {
                if (auto member_user = ctx.user_service.getUser(member_id)) {
                    if (!member_user->username.empty())
                        name = member_user->username;
                    else if (!member_user->full_name.empty())
                        name = member_user->full_name;
                }
            }
            if (name.empty() && member_id == user_id) name = display_name;
            if (name.empty()) name = "Member";

            members_json.push_back(
                {{"user_id", public_member.value}, {"display_name", name}, {"online", is_online}});
        }
        members_by_hub[hub_key] = std::move(members_json);
    }

    json auth_resp = {{"type", "auth_response"},
                      {"success", true},
                      {"user_id", public_user_id.value},
                      {"username", display_name},
                      {"hub_count", hubs.size()},
                      {"hubs", hubs_meta},
                      {"channels_by_hub", channels_by_hub},
                      {"members_by_hub", members_by_hub}};

    CommandSuccess res;
    res.intents.push_back(Unicast{.conn = input->conn, .payload = std::move(auth_resp)});

    // Notify online members in shared hubs that this user is now online
    for (const auto& hub : hubs) {
        const auto online_members = ctx.presence_manager.onlineUsersInHub(hub.id);
        std::vector<GlobalConnId> recipients;
        recipients.reserve(online_members.size());

        for (const auto& member_id : online_members) {
            if (member_id == user_id) continue;
            auto conn_exp = ctx.session_manager.getMainConnection(member_id);
            if (!conn_exp.has_value()) continue;
            recipients.push_back(conn_exp.value());
        }

        if (!recipients.empty()) {
            auto payload = ctx.hub_notifier.memberOnline(hub.id, user_id);
            if (!display_name.empty()) payload["display_name"] = display_name;
            payload["username"] = display_name;
            res.intents.push_back(
                Fanout{.conns = std::move(recipients), .payload = std::move(payload)});
        }
    }

    return res;
}

}  // namespace app
