#include "app/commands/SendMessageCommand.h"

#include "infra/persistence/chatdb.h"
#include "net/ClientGateway.h"
#include "net/PerSocketData.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

using nlohmann::json;

namespace app {

namespace {
std::string format_time_point(const std::chrono::system_clock::time_point& tp) {
    if (tp.time_since_epoch().count() == 0) return std::string{};
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm = *std::gmtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}
}

SendMessageCommand::SendMessageCommand(ChatDB& db, net::ClientGateway& gateway)
    : db_(db), gateway_(gateway) {}

void SendMessageCommand::execute(CommandContext& ctx) {
    auto& psd = ctx.psd;
    auto& output = ctx.output;
    const auto& data = ctx.input.data;

    if (!psd.authenticated) {
        output.success = false;
        output.error_code = "not_authenticated";
        output.error_message = "Authenticate before sending messages.";
        output.data = {{"type", "error"},
                       {"code", "not_authenticated"},
                       {"message", "Authentication required"}};
        return;
    }

    const std::string channel_id_str = data.value("channel_id", "");
    if (channel_id_str.empty()) {
        output.success = false;
        output.error_code = "missing_channel_id";
        output.error_message = "channel_id is required.";
        output.data = {{"type", "error"},
                       {"code", "missing_channel_id"},
                       {"message", "channel_id is required"}};
        return;
    }

    ChannelId channel_id{channel_id_str};
    if (psd.current_channel_id.value != channel_id.value) {
        // Ensure user is subscribed to the channel
        if (!psd.channel_subscriptions.count(channel_id)) {
            output.success = false;
            output.error_code = "not_in_channel";
            output.error_message = "Join the channel before sending messages.";
            output.data = {{"type", "error"},
                           {"code", "not_in_channel"},
                           {"message", "Join the channel before sending messages"}};
            return;
        }
    }

    std::string content = data.value("content", "");
    if (content.empty()) {
        output.success = false;
        output.error_code = "empty_message";
        output.error_message = "Message content cannot be empty.";
        output.data = {{"type", "error"},
                       {"code", "empty_message"},
                       {"message", "Message content cannot be empty"}};
        return;
    }

    if (content.size() > 4096) {
        output.success = false;
        output.error_code = "message_too_long";
        output.error_message = "Message content exceeds maximum length.";
        output.data = {{"type", "error"},
                       {"code", "message_too_long"},
                       {"message", "Message content exceeds maximum length"}};
        return;
    }

    auto db_message = db_.sendMessage(channel_id, psd.user_id, content);
    const std::string display = psd.username.empty() ? std::string("Member") : psd.username;
    json event = {{"type", "message"},
                  {"channel_id", db_message.channel_id.value},
                  {"sender", display},
                  {"content", db_message.text},
                  {"sent_at", format_time_point(db_message.sent_at)}};

    gateway_.publish(channel_topic(channel_id), event);

    output.success = true;
    output.error_code.clear();
    output.error_message.clear();
    output.data = {{"type", "send_message_ack"},
                   {"channel_id", channel_id.value}};
    output.sent_at = std::chrono::system_clock::now();
}

std::string SendMessageCommand::channel_topic(const ChannelId& channel_id) {
    return "channel:" + channel_id.value;
}

}  // namespace app
