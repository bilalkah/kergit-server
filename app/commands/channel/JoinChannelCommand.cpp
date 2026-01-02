#include "app/commands/channel/JoinChannelCommand.h"

#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "domains/Channel.h"
#include "domains/Message.h"

#include <nlohmann/json.hpp>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_set>

using nlohmann::json;

namespace app {

namespace {
std::string iso_time(const std::chrono::system_clock::time_point& tp) {
    if (tp.time_since_epoch().count() == 0) return std::string{};
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm = *std::gmtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

json build_members(CommandContext& ctx, const HubId& hub_id, const ChannelId& channel_id) {
    json arr = json::array();
    const auto members = ctx.hub_service.getHubMembers(hub_id);
    const auto online_users = ctx.presence_manager.onlineUsersInChannel(hub_id, channel_id);
    std::unordered_set<UserId> online_set(online_users.begin(), online_users.end());

    for (const auto& [member_id, stored_display] : members) {
        const auto public_member = ctx.ids.to_public(member_id);
        std::string name;
        if (auto u = ctx.user_service.getUser(member_id)) {
            name = u->username;
        }
        if (name.empty()) name = "Member";
        arr.push_back(json{{"handle", name},
                           {"display_name", name},
                           {"online", online_set.count(member_id) > 0},
                           {"user_id", public_member.value}});
    }
    return arr;
}

json build_history(CommandContext& ctx, const ChannelId& channel_id, int limit = 50) {
    json arr = json::array();
    auto history = ctx.channel_service.fetchMessages(channel_id, limit);
    const auto public_channel = ctx.ids.to_public(channel_id);

    // reverse to chronological order (oldest first)
    for (auto it = history.rbegin(); it != history.rend(); ++it) {
        std::string sender;
        if (auto u = ctx.user_service.getUser(it->sender_id)) {
            sender = u->username;
        }
        if (sender.empty()) sender = "Member";

        arr.push_back(json{{"channel_id", public_channel.value},
                           {"sender", sender},
                           {"content", it->text},
                           {"sent_at", iso_time(it->sent_at)}});
    }
    return arr;
}
}  // namespace

CommandResult JoinChannelCommand::execute(CommandContext& ctx, const CommandInput cmd) {
    const auto* input = std::get_if<JsonInput>(&cmd);
    if (!input) {
        return std::unexpected(CommandError{"invalid_input", "join_channel expects JSON input"});
    }

    auto user_exp = ctx.session_manager.sessionOfConnection(input->conn);
    if (!user_exp.has_value()) {
        return std::unexpected(CommandError{"not_authenticated", "Authenticate first"});
    }
    const UserId user_id = user_exp.value();

    const std::string channel_id_raw = input->body.value("channel_id", "");
    if (channel_id_raw.empty()) {
        return std::unexpected(CommandError{"missing_channel_id", "channel_id is required"});
    }

    auto channel_id_opt = ctx.ids.to_internal(PublicChannelId{channel_id_raw});
    if (!channel_id_opt.has_value()) {
        return std::unexpected(CommandError{"channel_not_found", "Channel not found"});
    }

    auto channel_opt = ctx.channel_service.getChannel(*channel_id_opt);
    if (!channel_opt.has_value()) {
        return std::unexpected(CommandError{"channel_not_found", "Channel not found"});
    }
    const Channel channel = channel_opt.value();
    const HubId hub_id = channel.hub_id;

    if (!ctx.hub_service.isHubMember(hub_id, user_id)) {
        return std::unexpected(CommandError{"not_in_hub", "Join the hub before joining channels"});
    }

    // Unsubscribe from previous channel if present
    auto session = ctx.session_manager.getSession(user_id);
    if (session.has_value() && session->current_text_channel) {
        auto prev = session->current_text_channel.value();
        ctx.subscription_manager.unsubscribe(user_id, Topic::ChannelTopic(hub_id, prev));
    }

    // Subscribe user to channel topic and record session context
    ctx.subscription_manager.subscribe(user_id, Topic::ChannelTopic(hub_id, channel.id));
    ctx.session_manager.joinTextChannel(user_id, hub_id, channel.id);

    json payload = json{{"type", "joined_channel"},
                        {"hub_id", ctx.ids.to_public(hub_id).value},
                        {"channel_id", channel_id_raw},
                        {"channel_name", channel.name},
                        {"members", build_members(ctx, hub_id, channel.id)},
                        {"history", build_history(ctx, channel.id)}};

    CommandSuccess res;
    res.intents.push_back(Unicast{.conn = input->conn, .payload = std::move(payload)});
    return res;
}

}  // namespace app
