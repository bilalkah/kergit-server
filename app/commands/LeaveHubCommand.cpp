#include "app/commands/LeaveHubCommand.h"

#include "app/services/HubPublisher.h"
#include "app/services/PublicIdService.h"
#include "infra/persistence/PersistenceGateway.h"
#include "net/ClientGateway.h"
#include "net/ConnectionManager.h"
#include "net/PerSocketData.h"

#include <nlohmann/json.hpp>
#include <string>
#include <unordered_set>
#include <vector>

using nlohmann::json;

namespace app {

LeaveHubCommand::LeaveHubCommand(PersistenceGateway& db, net::ClientGateway& gateway,
                                 net::ConnectionManager& connections,
                                 app::services::HubPublisher& hub_publisher,
                                 app::services::PublicIdService& ids)
    : db_(db),
      gateway_(gateway),
      connections_(connections),
      hub_publisher_(hub_publisher),
      ids_(ids) {}

bool LeaveHubCommand::is_owner(const CommandContext& ctx, const HubId& hub_id) {
    auto it = ctx.snapshot.roles.find(hub_id);
    if (it == ctx.snapshot.roles.end()) return false;

    Role role = it->second;
    return role == Role::OWNER;
}

std::string LeaveHubCommand::channel_topic(const ChannelId& channel_id) {
    return "channel:" + channel_id.value;
}

void LeaveHubCommand::publish_presence_update(const ChannelId& channel_id, CommandContext& ctx,
                                              bool online) {
    if (channel_id.value.empty()) return;
    const auto name = ctx.username;
    if (!name.empty()) ids_.remember_display(ctx.user_id, name);
    const auto public_channel_id = ids_.to_public(channel_id);
    const auto public_user_id = ids_.to_public(ctx.user_id);
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

void LeaveHubCommand::execute(CommandContext& ctx) {
    const auto& input = ctx.input;
    auto& output = ctx.output;

    if (!ctx.authenticated) {
        output.success = false;
        output.error_code = "not_authenticated";
        output.error_message = "Authentication required.";
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

    const std::string hub_id_str = input.data.value("hub_id", "");
    if (hub_id_str.empty()) {
        output.success = false;
        output.error_code = "missing_hub_id";
        output.error_message = "hub_id is required.";
        json err = {
            {"type", "error"}, {"code", "missing_hub_id"}, {"message", "hub_id is required"}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
        return;
    }

    auto internal_hub = ids_.to_internal(PublicHubId{hub_id_str});
    if (!internal_hub.has_value()) {
        output.success = false;
        output.error_code = "hub_not_found";
        output.error_message = "Hub not found.";
        json err = {
            {"type", "error"}, {"code", "hub_not_found"}, {"message", "Hub does not exist"}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
        return;
    }

    if (!ctx.snapshot.hubs.count(*internal_hub) &&
        !db_.hubs().isHubMember(*internal_hub, ctx.user_id)) {
        output.success = false;
        output.error_code = "not_in_hub";
        output.error_message = "Join the hub before leaving it.";
        json err = {{"type", "error"},
                    {"code", "not_in_hub"},
                    {"message", "Join the hub before leaving it"}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
        return;
    }

    if (is_owner(ctx, *internal_hub)) {
        output.success = false;
        output.error_code = "hub_owner_must_transfer";
        output.error_message = "Owners must transfer ownership before leaving.";
        json err = {{"type", "error"},
                    {"code", "hub_owner_must_transfer"},
                    {"message", "Transfer ownership or delete the hub before leaving"}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
        return;
    }

    try {
        const auto public_hub_id = ids_.to_public(*internal_hub);
        const auto channels = db_.channels().getHubChannels(*internal_hub);
        db_.hubs().removeMember(*internal_hub, ctx.user_id);

        const auto hub_list =
            gateway_.subscribers(app::services::HubPublisher::topic_for(*internal_hub));
        gateway_.unsubscribe(ctx.conn_id, app::services::HubPublisher::topic_for(*internal_hub));
        for (const auto& channel : channels) {
            gateway_.unsubscribe(ctx.conn_id, channel_topic(channel.channel_id));
            publish_presence_update(channel.channel_id, ctx, false);
        }

        hub_publisher_.publish_hub(*internal_hub);

        output.success = true;
        output.error_code.clear();
        output.error_message.clear();
        json data = {{"type", "hub_left"}, {"hub_id", public_hub_id.value}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = data.dump();
        msg.apply_psd = [internal_hub = *internal_hub,
                         &connections = connections_](net::PerSocketData* psd) {
            auto snapshot = *psd->snapshot;
            psd->current_hub_id = HubId{""};
            psd->current_channel_id = ChannelId{""};
            snapshot.hubs.erase(internal_hub);
            snapshot.roles.erase(internal_hub);
            psd->snapshot = std::make_shared<const net::Snapshot>(std::move(snapshot));
        };
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
    } catch (const std::exception& ex) {
        output.success = false;
        output.error_code = "leave_hub_failed";
        output.error_message = ex.what();
        json err = {{"type", "error"}, {"code", "leave_hub_failed"}, {"message", ex.what()}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
    }
}

}  // namespace app
