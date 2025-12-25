#include "app/commands/SendMessageCommand.h"

#include "net/PerSocketData.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <nlohmann/json.hpp>
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
}  // namespace

SendMessageCommand::SendMessageCommand(ServiceObjects& svc_objs)
    : services_(svc_objs) {}

void SendMessageCommand::execute(CommandContext& ctx) {
    const auto& input = ctx.input;
    auto& output = ctx.output;

    if (!ctx.snapshot.authenticated) {
        output.success = false;
        output.error_code = "not_authenticated";
        output.error_message = "Authenticate before sending messages.";
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
        output.error_message = "channel_id is required.";
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
        output.error_message = "Channel does not exist.";
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
    if (ctx.snapshot.current_text_channel_id.value != channel_id.value) {
        // Ensure user is subscribed to the channel
        const auto sub_list = services_.gateway_.subscribers(channel_topic(channel_id));

        if (!sub_list.count(ctx.conn_id)) {
            output.success = false;
            output.error_code = "not_in_channel";
            output.error_message = "Join the channel before sending messages.";
            json err = {{"type", "error"},
                        {"code", "not_in_channel"},
                        {"message", "Join the channel before sending messages"}};
            DirectMessage msg;
            msg.conn_id = ctx.conn_id;
            msg.payload = err.dump();
            ctx.output.messages.push_back(std::move(msg));
            output.sent_at = std::chrono::system_clock::now();
            return;
        }
    }

    std::string content = input.data.value("content", "");
    if (content.empty()) {
        output.success = false;
        output.error_code = "empty_message";
        output.error_message = "Message content cannot be empty.";
        json err = {{"type", "error"},
                    {"code", "empty_message"},
                    {"message", "Message content cannot be empty"}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
        return;
    }

    if (content.size() > 4096) {
        output.success = false;
        output.error_code = "message_too_long";
        output.error_message = "Message content exceeds maximum length.";
        json err = {{"type", "error"},
                    {"code", "message_too_long"},
                    {"message", "Message content exceeds maximum length"}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
        return;
    }

    auto db_message = services_.db_.channels().sendMessage(channel_id, ctx.snapshot.user_id, content);
    const auto public_channel_id = services_.ids_.to_public(channel_id);
    std::string display = ctx.snapshot.username;
    if (display.empty()) display = services_.ids_.display_for(ctx.snapshot.user_id);
    if (display.empty()) {
        if (auto db_name = services_.db_.users().getUserDisplayName(ctx.snapshot.user_id)) display = *db_name;
    }
    if (display.empty()) display = "Member";
    services_.ids_.remember_display(ctx.snapshot.user_id, display);
    json event = {{"type", "message"},
                  {"channel_id", public_channel_id.value},
                  {"sender", display},
                  {"content", db_message.text},
                  {"sent_at", format_time_point(db_message.sent_at)}};

    PublishMessage msg;
    msg.topic = channel_topic(channel_id);
    msg.payload = event.dump();
    ctx.output.messages.push_back(std::move(msg));

    output.success = true;
    output.error_code.clear();
    output.error_message.clear();
    json data = {{"type", "send_message_ack"}, {"channel_id", public_channel_id.value}};
    DirectMessage ack;
    ack.conn_id = ctx.conn_id;
    ack.payload = data.dump();
    ctx.output.messages.push_back(std::move(ack));
    output.sent_at = std::chrono::system_clock::now();
}

std::string SendMessageCommand::channel_topic(const ChannelId& channel_id) {
    return "channel:" + channel_id.value;
}

}  // namespace app
