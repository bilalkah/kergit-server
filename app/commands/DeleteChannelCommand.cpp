#include "app/commands/DeleteChannelCommand.h"

#include "app/services/HubPublisher.h"
#include "app/services/PublicIdService.h"
#include "infra/persistence/PersistenceGateway.h"
#include "net/ClientGateway.h"
#include "net/ConnectionManager.h"
#include "net/PerSocketData.h"

#include <nlohmann/json.hpp>
#include <unordered_set>
#include <vector>

using nlohmann::json;

namespace app {

DeleteChannelCommand::DeleteChannelCommand(PersistenceGateway& db, net::ClientGateway& gateway,
                                           net::ConnectionManager& connections,
                                           app::services::HubPublisher& hub_publisher,
                                           app::services::PublicIdService& ids)
    : db_(db),
      gateway_(gateway),
      connections_(connections),
      hub_publisher_(hub_publisher),
      ids_(ids) {}

std::string DeleteChannelCommand::channel_topic(const ChannelId& channel_id) {
    return "channel:" + channel_id.value;
}

bool DeleteChannelCommand::has_privilege(const CommandContext& ctx, const HubId& hub_id) {
    auto it = ctx.snapshot.roles.find(hub_id);
    if (it == ctx.snapshot.roles.end()) return false;

    Role role = it->second;
    return role == Role::OWNER || role == Role::ADMIN;
}

void DeleteChannelCommand::execute(CommandContext& ctx) {
    const auto& input = ctx.input;
    auto output = ctx.output;

    if (!ctx.authenticated) {
        output.success = false;
        output.error_code = "not_authenticated";
        output.error_message = "Authenticate before deleting channels.";
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

    auto internal_channel = ids_.to_internal(PublicChannelId{channel_id_str});
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

    auto channel = db_.channels().getChannel(*internal_channel);
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

    const HubId hub_id = channel->hub_id;
    if (!has_privilege(ctx, hub_id)) {
        output.success = false;
        output.error_code = "insufficient_privilege";
        output.error_message = "Only owners or admins can delete channels.";
        json err = {{"type", "error"},
                    {"code", "insufficient_privilege"},
                    {"message", "Only owners or admins can delete channels"}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
        return;
    }

    try {
        if (!db_.channels().deleteChannel(channel->channel_id, hub_id)) {
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

        const auto public_hub_id = ids_.to_public(hub_id);
        const auto public_channel_id = ids_.to_public(channel->channel_id);
        json channel_deleted_msg = {{"type", "channel_deleted"},
                                    {"hub_id", public_hub_id.value},
                                    {"channel_id", public_channel_id.value}};
        json channel_closed_msg = {{"type", "channel_closed"},
                                   {"channel_id", public_channel_id.value}};

        const auto subs = gateway_.subscribers(channel_topic(channel->channel_id));

        for (const auto& cid : subs) {
            DirectMessage ch_deleted, ch_closed;
            ch_deleted.conn_id = cid;
            ch_deleted.payload = channel_deleted_msg.dump();
            ctx.output.messages.push_back(std::move(ch_deleted));

            ch_closed.conn_id = cid;
            ch_closed.payload = channel_closed_msg.dump();
            ctx.output.messages.push_back(std::move(ch_closed));
        }

        gateway_.drop_topic(channel_topic(channel->channel_id));

        hub_publisher_.publish_hub(hub_id);

        output.success = true;
        output.error_code.clear();
        output.error_message.clear();
        json data = {{"type", "channel_deleted"},
                     {"hub_id", public_hub_id.value},
                     {"channel_id", public_channel_id.value}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = data.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
    } catch (const std::exception& ex) {
        output.success = false;
        output.error_code = "delete_failed";
        output.error_message = ex.what();
        json err = {{"type", "error"}, {"code", "delete_failed"}, {"message", ex.what()}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
    }
}

}  // namespace app
