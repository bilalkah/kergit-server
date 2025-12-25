#include "app/commands/JoinChannelCommand.h"

#include "net/PerSocketData.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

using nlohmann::json;

namespace app {
namespace {
constexpr int kHistoryLimit = 50;

std::string format_time_point(const std::chrono::system_clock::time_point& tp) {
    if (tp.time_since_epoch().count() == 0) return std::string{};
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm = *std::gmtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}
}  // namespace

JoinChannelCommand::JoinChannelCommand(ServiceObjects& svc_objs)
    : services_(svc_objs) {}

void JoinChannelCommand::execute(CommandContext& ctx) {
    const auto& input = ctx.input;
    auto& output = ctx.output;

    if (!ctx.snapshot.authenticated) {
        output.success = false;
        output.error_code = "not_authenticated";
        output.error_message = "Authenticate before joining a channel.";
        json err = {{"type", "error"},
                    {"code", "not_authenticated"},
                    {"message", "Authentication required"}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
        return;
    }

    const std::string channel_id_str = input.data.value("channel_id", "");
    if (channel_id_str.empty()) {
        output.success = false;
        output.error_code = "missing_channel_id";
        output.error_message = "Channel id is required.";
        json err = {{"type", "error"},
                    {"code", "missing_channel_id"},
                    {"message", "channel_id is required"}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
        return;
    }

    auto channel_id_opt = services_.ids_.to_internal(PublicChannelId{channel_id_str});
    if (!channel_id_opt.has_value()) {
        output.success = false;
        output.error_code = "channel_not_found";
        output.error_message = "Channel does not exist";
        json err = {{"type", "error"},
                    {"code", "channel_not_found"},
                    {"message", "Channel does not exist"}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
        return;
    }

    ChannelId channel_id = *channel_id_opt;
    auto channel_info_opt = services_.db_.channels().getChannel(channel_id);
    if (!channel_info_opt.has_value()) {
        output.success = false;
        output.error_code = "channel_not_found";
        output.error_message = "Channel not found.";
        json err = {{"type", "error"},
                    {"code", "channel_not_found"},
                    {"message", "Channel does not exist"}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
        return;
    }

    const Channel& channel_info = channel_info_opt.value();
    const HubId hub_id = channel_info.hub_id;
    const auto public_channel_id = services_.ids_.to_public(channel_id);
    const auto public_hub_id = services_.ids_.to_public(hub_id);

    if (!ctx.snapshot.hubs.count(hub_id)) {
        if (!services_.db_.hubs().isHubMember(hub_id, ctx.snapshot.user_id)) {
            output.success = false;
            output.error_code = "not_in_hub";
            output.error_message = "User is not a member of the hub.";
            json err = {{"type", "error"},
                        {"code", "not_in_hub"},
                        {"message", "You are not a member of this hub"}};
            DirectMessage msg;
            msg.conn_id = ctx.conn_id;
            msg.payload = err.dump();
            ctx.output.messages.push_back(std::move(msg));
            output.sent_at = std::chrono::system_clock::now();
            return;
        }
    }

    ChannelId previous_channel = ctx.snapshot.current_text_channel_id;
    if (!previous_channel.value.empty() && previous_channel.value != channel_id.value) {
        // publish_presence_update(previous_channel, ctx, false);
        services_.gateway_.unsubscribe(ctx.conn_id, channel_topic(previous_channel));
    }

    auto topic = channel_topic(channel_id);
    services_.gateway_.subscribe(ctx.conn_id, topic);

    auto history = fetch_history(channel_id);
    std::reverse(history.begin(), history.end());
    json history_json = json::array();
    for (const auto& msg : history) {
        const auto sent_at = format_time_point(msg.sent_at);
        history_json.push_back({{"channel_id", public_channel_id.value},
                                {"sender", resolve_display_name(msg.sender_id)},
                                {"content", msg.text},
                                {"sent_at", sent_at}});
    }

    auto members = collect_channel_presence(channel_id, ctx);
    // publish_presence_update(channel_id, ctx, true);

    output.success = true;
    output.error_code.clear();
    output.error_message.clear();
    json data = {{"type", "joined_channel"},          {"channel_id", public_channel_id.value},
                 {"channel_name", channel_info.name}, {"hub_id", public_hub_id.value},
                 {"members", std::move(members)},     {"history", std::move(history_json)}};
    DirectMessage msg;
    msg.conn_id = ctx.conn_id;
    msg.payload = data.dump();
    msg.apply_psd = [channel_id, hub_id](net::PerSocketData* psd) {
        if (psd) {
            auto snapshot = *psd->snapshot;
            snapshot.current_text_channel_id = channel_id;
            snapshot.current_hub_id = hub_id;
            psd->snapshot = std::make_shared<const net::Snapshot>(std::move(snapshot));
        }
    };
    ctx.output.messages.push_back(std::move(msg));
    output.sent_at = std::chrono::system_clock::now();
}

json JoinChannelCommand::collect_channel_presence(const ChannelId& channel_id,
                                                  CommandContext& ctx) const {
    json members = json::array();
    const auto conn_list = services_.gateway_.subscribers(channel_topic(channel_id));

    for (const auto& cid : conn_list) {
        const auto public_user = services_.ids_.to_public(ctx.snapshot.user_id);
        const auto name = ctx.snapshot.username.empty() ? "Member" : ctx.snapshot.username;
        members.push_back({{"handle", name},
                           {"display_name", name},
                           {"online", true},
                           {"user_id", public_user.value}});
    }
    return members;
}

std::vector<Message> JoinChannelCommand::fetch_history(const ChannelId& channel_id) {
    return services_.db_.channels().fetchMessages(channel_id, kHistoryLimit);
}

std::string JoinChannelCommand::channel_topic(const ChannelId& channel_id) {
    return "channel:" + channel_id.value;
}

std::string JoinChannelCommand::resolve_display_name(const UserId& user_id) const {
    std::string name = services_.ids_.display_for(user_id);

    if (name.empty()) {
        if (auto db_name = services_.db_.users().getUserDisplayName(user_id)) {
            if (!db_name->empty()) {
                name = *db_name;
                services_.ids_.remember_display(user_id, name);
            }
        }
    }
    if (name.empty()) name = "Member";
    return name;
}

void JoinChannelCommand::publish_presence_update(const ChannelId& channel_id, CommandContext& ctx,
                                                 bool online) {
    if (channel_id.value.empty()) return;
    const auto name = ctx.snapshot.username.empty() ? "Member" : ctx.snapshot.username;
    if (!name.empty()) services_.ids_.remember_display(ctx.snapshot.user_id, name);
    const auto public_channel_id = services_.ids_.to_public(channel_id);
    const auto public_user_id = services_.ids_.to_public(ctx.snapshot.user_id);
    json payload = {{"type", "presence_update"},
                    {"channel_id", public_channel_id.value},
                    {"handle", name},
                    {"display_name", name},
                    {"online", online},
                    {"user_id", public_user_id.value}};
    PublishMessage msg;
    msg.topic = channel_topic(channel_id);
    msg.payload = payload.dump();
    ctx.output.messages.push_back(std::move(msg));
}

}  // namespace app
