#include "app/commands/JoinChannelCommand.h"

#include "app/services/PublicIdService.h"
#include "infra/persistence/PersistenceGateway.h"
#include "net/ClientGateway.h"
#include "net/ConnectionManager.h"
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

std::string display_from_psd(const net::PerSocketData& psd) {
    if (!psd.username.empty()) return psd.username;
    return "Member";
}

std::string format_time_point(const std::chrono::system_clock::time_point& tp) {
    if (tp.time_since_epoch().count() == 0) return std::string{};
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm = *std::gmtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}
}  // namespace

JoinChannelCommand::JoinChannelCommand(PersistenceGateway& db, net::ClientGateway& gateway,
                                       net::ConnectionManager& connections,
                                       app::services::PublicIdService& ids)
    : db_(db), gateway_(gateway), connections_(connections), ids_(ids) {}

void JoinChannelCommand::execute(CommandContext& ctx) {
    auto& psd = ctx.psd;
    auto& input = ctx.input;
    auto& output = ctx.output;

    if (!psd.authenticated) {
        output.success = false;
        output.error_code = "not_authenticated";
        output.error_message = "Authenticate before joining a channel.";
        output.data = {{"type", "error"},
                       {"code", "not_authenticated"},
                       {"message", "Authentication required"}};
        return;
    }

    const std::string channel_id_str = input.data.value("channel_id", "");
    if (channel_id_str.empty()) {
        output.success = false;
        output.error_code = "missing_channel_id";
        output.error_message = "Channel id is required.";
        output.data = {{"type", "error"},
                       {"code", "missing_channel_id"},
                       {"message", "channel_id is required"}};
        return;
    }

    auto channel_id_opt = ids_.to_internal(PublicChannelId{channel_id_str});
    if (!channel_id_opt.has_value()) {
        output.success = false;
        output.error_code = "channel_not_found";
        output.error_message = "Channel does not exist";
        output.data = {{"type", "error"},
                       {"code", "channel_not_found"},
                       {"message", "Channel does not exist"}};
        return;
    }

    ChannelId channel_id = *channel_id_opt;
    auto channel_info_opt = db_.channels().getChannel(channel_id);
    if (!channel_info_opt.has_value()) {
        output.success = false;
        output.error_code = "channel_not_found";
        output.error_message = "Channel not found.";
        output.data = {{"type", "error"},
                       {"code", "channel_not_found"},
                       {"message", "Channel does not exist"}};
        return;
    }

    const Channel& channel_info = channel_info_opt.value();
    const HubId hub_id = channel_info.hub_id;
    const auto public_channel_id = ids_.to_public(channel_id);
    const auto public_hub_id = ids_.to_public(hub_id);

    if (!psd.hub_memberships.count(hub_id)) {
        if (!db_.hubs().isHubMember(hub_id, psd.user_id)) {
            output.success = false;
            output.error_code = "not_in_hub";
            output.error_message = "User is not a member of the hub.";
            output.data = {{"type", "error"},
                           {"code", "not_in_hub"},
                           {"message", "You are not a member of this hub"}};
            return;
        }
        psd.hub_memberships.insert(hub_id);
    }

    ChannelId previous_channel = psd.current_channel_id;
    if (!previous_channel.value.empty() && previous_channel.value != channel_id.value) {
        publish_presence_update(previous_channel, psd, false);
        gateway_.unsubscribe(psd.conn_id, channel_topic(previous_channel));
        psd.channel_subscriptions.erase(previous_channel);
    }

    psd.current_hub_id = hub_id;
    psd.current_channel_id = channel_id;

    auto topic = channel_topic(channel_id);
    gateway_.subscribe(psd.conn_id, topic);
    psd.channel_subscriptions.insert(channel_id);

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

    auto members = collect_channel_presence(channel_id);
    publish_presence_update(channel_id, psd, true);

    output.success = true;
    output.error_code.clear();
    output.error_message.clear();
    output.data = {
        {"type", "joined_channel"},          {"channel_id", public_channel_id.value},
        {"channel_name", channel_info.name}, {"hub_id", public_hub_id.value},
        {"members", std::move(members)},     {"history", std::move(history_json)},
    };
    output.sent_at = std::chrono::system_clock::now();
}

json JoinChannelCommand::collect_channel_presence(const ChannelId& channel_id) const {
    json members = json::array();
    connections_.for_each([&](UwsSocket* ws) {
        if (!ws) return;
        auto* other = ws->getUserData();
        if (!other || !other->authenticated) return;
        if (other->current_channel_id.value != channel_id.value) return;
        const auto name = display_from_psd(*other);
        if (!name.empty()) ids_.remember_display(other->user_id, name);
        const auto public_user = ids_.to_public(other->user_id);
        members.push_back({{"handle", name},
                           {"display_name", name},
                           {"online", true},
                           {"user_id", public_user.value}});
    });
    return members;
}

std::vector<Message> JoinChannelCommand::fetch_history(const ChannelId& channel_id) {
    return db_.channels().fetchMessages(channel_id, kHistoryLimit);
}

std::string JoinChannelCommand::channel_topic(const ChannelId& channel_id) {
    return "channel:" + channel_id.value;
}

std::string JoinChannelCommand::resolve_display_name(const UserId& user_id) const {
    std::string name;
    connections_.for_each([&](UwsSocket* ws) {
        if (name.empty() && ws) {
            auto* other = ws->getUserData();
            if (other && other->authenticated && other->user_id == user_id) {
                name = display_from_psd(*other);
            }
        }
    });
    if (name.empty()) {
        name = ids_.display_for(user_id);
    }
    if (name.empty()) {
        if (auto db_name = db_.users().getUserDisplayName(user_id)) {
            if (!db_name->empty()) {
                name = *db_name;
                ids_.remember_display(user_id, name);
            }
        }
    }
    if (name.empty()) name = "Member";
    return name;
}

void JoinChannelCommand::publish_presence_update(const ChannelId& channel_id,
                                                 const net::PerSocketData& psd, bool online) {
    if (channel_id.value.empty()) return;
    const auto name = display_from_psd(psd);
    if (!name.empty()) ids_.remember_display(psd.user_id, name);
    const auto public_channel_id = ids_.to_public(channel_id);
    const auto public_user_id = ids_.to_public(psd.user_id);
    json payload = {{"type", "presence_update"},
                    {"channel_id", public_channel_id.value},
                    {"handle", name},
                    {"display_name", name},
                    {"online", online},
                    {"user_id", public_user_id.value}};
    gateway_.publish(channel_topic(channel_id), payload);
}

}  // namespace app
