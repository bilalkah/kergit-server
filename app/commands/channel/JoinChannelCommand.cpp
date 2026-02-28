#include "app/commands/channel/JoinChannelCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "domains/Channel.h"
#include "domains/Message.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>
#include <string>

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

json build_history(CommandContext& ctx, const ChannelId& channel_id, int limit = 50) {
    json arr = json::array();
    auto history_exp = ctx.channel_service.fetchMessages(channel_id, limit);
    if (!history_exp.has_value()) {
        return arr;
    }
    const auto& history = history_exp.value();

    // reverse to chronological order (oldest first)
    for (auto it = history.rbegin(); it != history.rend(); ++it) {
        std::string sender;
        if (auto u = ctx.user_service.getUser(it->sender_id)) {
            sender = u->username;
        }
        if (sender.empty()) sender = "Member";

        arr.push_back(json{{"channel_id", channel_id.value},
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
        return std::unexpected(CommandError{1, "join_channel expects JSON input"});
    }

    auto user_exp = ctx.session_manager.sessionOfConnection(input->conn);
    if (!user_exp.has_value()) {
        return std::unexpected(CommandError{2, "Authenticate first"});
    }
    const UserId user_id = user_exp.value();

    const auto j = json::parse(input->body, nullptr, false);
    if (j.is_discarded()) {
        return std::unexpected(CommandError{3, "Invalid JSON"});
    }

    const std::string channel_id_raw = j.value("channel_id", "");
    if (channel_id_raw.empty()) {
        return std::unexpected(CommandError{3, "channel_id is required"});
    }

    auto channel_id_opt = parse_wire_id<ChannelId>(channel_id_raw);
    if (!channel_id_opt.has_value()) {
        return std::unexpected(CommandError{4, "Channel not found"});
    }

    auto channel_opt = ctx.channel_service.getChannel(*channel_id_opt);
    if (!channel_opt.has_value()) {
        return std::unexpected(CommandError{5, "Channel not found"});
    }
    const Channel channel = channel_opt.value();
    const HubId hub_id = channel.hub_id;

    if (!ctx.hub_service.isHubMember(hub_id, user_id)) {
        return std::unexpected(CommandError{6, "Join the hub before joining channels"});
    }

    unsubscribe_connection_from_channel_topics(ctx.subscription_manager, input->conn);

    // Subscribe user to channel topic and record session context
    ctx.subscription_manager.subscribeConnection(
        input->conn, Topic::ChannelTopic(hub_id, channel.id));
    ctx.session_manager.joinTextChannel(user_id, hub_id);

    json payload = json{{"type", "joined_channel"},
                        {"hub_id", hub_id.value},
                        {"channel_id", channel.id.value},
                        {"channel_name", channel.name},
                        {"history", build_history(ctx, channel.id)}};

    CommandSuccess res;
    res.intents.push_back(Unicast{.conn = input->conn, .payload = std::move(payload)});
    return res;
}

}  // namespace app
