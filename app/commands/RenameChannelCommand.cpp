#include "app/commands/RenameChannelCommand.h"

#include "net/PerSocketData.h"

#include <algorithm>
#include <cctype>
#include <nlohmann/json.hpp>
#include <string>

using nlohmann::json;

namespace app {

namespace {
std::string channel_type_to_string(ChannelType type) {
    return type == ChannelType::VOICE ? "voice" : "text";
}
}  // namespace

RenameChannelCommand::RenameChannelCommand(ServiceObjects& svc_objs)
    : services_(svc_objs) {}

std::string RenameChannelCommand::sanitize(std::string name) {
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

bool RenameChannelCommand::has_privilege(const CommandContext& ctx, const HubId& hub_id) {
    auto role = services_.db_.hubs().getMembershipRole(hub_id, ctx.snapshot.user_id);
    if (!role.has_value()) {
        return false;
    }
    return *role == Role::OWNER || *role == Role::ADMIN;
}

void RenameChannelCommand::execute(CommandContext& ctx) {
    const auto& input = ctx.input;
    auto& output = ctx.output;

    if (!ctx.snapshot.authenticated) {
        output.success = false;
        output.error_code = "not_authenticated";
        output.error_message = "Authentication required";
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
    std::string requested_name = input.data.value("name", std::string{});

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

    requested_name = sanitize(std::move(requested_name));
    if (requested_name.empty()) {
        output.success = false;
        output.error_code = "invalid_channel_name";
        output.error_message = "Channel name is required.";
        json err = {{"type", "error"},
                    {"code", "invalid_channel_name"},
                    {"message", "Channel name is required"}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
        return;
    }

    auto internal_channel = services_.ids_.to_internal(PublicChannelId{channel_id_str});
    if (!internal_channel.has_value()) {
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

    auto channel = services_.db_.channels().getChannel(*internal_channel);
    if (!channel.has_value()) {
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

    if (!ctx.snapshot.hubs.count(channel->hub_id)) {
        if (!services_.db_.hubs().isHubMember(channel->hub_id, ctx.snapshot.user_id)) {
            output.success = false;
            output.error_code = "not_in_hub";
            output.error_message = "Join the hub before renaming channels.";
            json err = {{"type", "error"},
                        {"code", "not_in_hub"},
                        {"message", "Join the hub before renaming channels"}};
            DirectMessage msg;
            msg.conn_id = ctx.conn_id;
            msg.payload = err.dump();
            ctx.output.messages.push_back(std::move(msg));
            output.sent_at = std::chrono::system_clock::now();
            return;
        }
        // psd.hub_memberships.insert(channel->hub_id);
    }

    if (!has_privilege(ctx, channel->hub_id)) {
        output.success = false;
        output.error_code = "insufficient_privilege";
        output.error_message = "Only owners or admins can rename channels.";
        json err = {{"type", "error"},
                    {"code", "insufficient_privilege"},
                    {"message", "Only owners or admins can rename channels"}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
        return;
    }

    try {
        if (!services_.db_.channels().renameChannel(channel->id, requested_name)) {
            output.success = false;
            output.error_code = "rename_channel_failed";
            output.error_message = "Unable to rename channel.";
            json err = {{"type", "error"},
                        {"code", "rename_channel_failed"},
                        {"message", "Unable to rename channel"}};
            DirectMessage msg;
            msg.conn_id = ctx.conn_id;
            msg.payload = err.dump();
            ctx.output.messages.push_back(std::move(msg));
            output.sent_at = std::chrono::system_clock::now();
            return;
        }

        const auto public_channel_id = services_.ids_.to_public(channel->id);
        const auto public_hub_id = services_.ids_.to_public(channel->hub_id);

        nlohmann::json channel_json = {{"id", public_channel_id.value},
                                       {"hub_id", public_hub_id.value},
                                       {"name", requested_name},
                                       {"type", channel_type_to_string(channel->type)}};

        services_.hub_publisher_.publish_hub(channel->hub_id);

        output.success = true;
        output.error_code.clear();
        output.error_message.clear();
        json data = {{"type", "channel_renamed"},
                     {"hub_id", public_hub_id.value},
                     {"channel", channel_json}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = data.dump();
        if (!ctx.snapshot.hubs.count(channel->hub_id)) {
            msg.apply_psd = [hub_id = channel->hub_id](net::PerSocketData* psd) {
                if (psd) {
                    auto snapshot = *psd->snapshot;
                    snapshot.hubs.insert(hub_id);
                    snapshot.roles[hub_id] = Role::USER;
                    psd->snapshot = std::make_shared<const net::Snapshot>(std::move(snapshot));
                }
            };
        }
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
    } catch (const std::exception& ex) {
        output.success = false;
        output.error_code = "rename_channel_failed";
        output.error_message = ex.what();
        json err = {{"type", "error"}, {"code", "rename_channel_failed"}, {"message", ex.what()}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
    }
}

}  // namespace app
