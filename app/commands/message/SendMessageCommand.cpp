#include "app/commands/message/SendMessageCommand.h"

#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "domains/Message.h"

#include <nlohmann/json.hpp>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_set>
#include <thread>

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

}  // namespace

CommandResult SendMessageCommand::execute(CommandContext& ctx, const CommandInput cmd) {
    const auto* input = std::get_if<JsonInput>(&cmd);
    if (!input) {
        return std::unexpected(CommandError{"invalid_input", "send_message expects JSON input"});
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

    std::string content = input->body.value("content", "");
    if (content.empty()) {
        return std::unexpected(CommandError{"empty_message", "Message content cannot be empty"});
    }
    if (content.size() > 4096) {
        return std::unexpected(CommandError{"message_too_long", "Message exceeds maximum length"});
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

    if (!ctx.hub_service.isHubMember(channel.hub_id, user_id)) {
        return std::unexpected(CommandError{"not_in_hub", "Join the hub before sending messages"});
    }

    auto subbed = ctx.subscription_manager.isSubscribed(user_id,
                                                        Topic::ChannelTopic(channel.hub_id, channel.id));
    if (!subbed) {
        return std::unexpected(
            CommandError{"not_in_channel", "Join the channel before sending messages"});
    }

    const auto public_channel_id = ctx.ids.to_public(channel.id);
    auto sender = std::string("Member");
    if (auto u = ctx.user_service.getUser(user_id)) {
        if (!u->username.empty()) sender = u->username;
    }

    auto sent_at = std::chrono::system_clock::now();
    json event = {{"type", "message"},
                  {"channel_id", public_channel_id.value},
                  {"sender", sender},
                  {"content", content},
                  {"sent_at", iso_time(sent_at)}};

    // Persist message asynchronously to avoid blocking the worker
    // TODO: replace with a lightweight shared worker/queue instead of spawning per message.
    auto* channel_service = &ctx.channel_service;
    std::thread([channel_service, channel_id = channel.id, user_id, body = content, sent_at]() {
        try {
            channel_service->sendMessage(channel_id, user_id, body);
        } catch (...) {
            // swallow persistence errors for now
        }
    }).detach();

    CommandSuccess res;
    res.intents.push_back(
        Unicast{.conn = input->conn,
                .payload = json{{"type", "send_message_ack"}, {"channel_id", public_channel_id.value}}});

    auto subs = ctx.subscription_manager.getSubscribers(Topic::ChannelTopic(channel.hub_id, channel.id));
    if (subs.has_value() && !subs->empty()) {
        std::vector<GlobalConnId> conns;
        conns.reserve(subs->size());
        for (const auto& uid : subs.value()) {
            if (uid == user_id) continue;  // sender already receives the ack above
            auto conn = ctx.session_manager.getMainConnection(uid);
            if (conn.has_value()) conns.push_back(conn.value());
        }
        if (!conns.empty()) {
            res.intents.push_back(Fanout{.conns = std::move(conns), .payload = event});
        }
    }

    return res;
}

}  // namespace app
