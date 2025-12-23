#include "app/commands/DeleteHubCommand.h"

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

DeleteHubCommand::DeleteHubCommand(PersistenceGateway& db, net::ClientGateway& gateway,
                                   net::ConnectionManager& connections,
                                   app::services::HubPublisher& hub_publisher,
                                   app::services::PublicIdService& ids)
    : db_(db),
      gateway_(gateway),
      connections_(connections),
      hub_publisher_(hub_publisher),
      ids_(ids) {}

bool DeleteHubCommand::is_owner(const CommandContext& ctx, const HubId& hub_id) {
    auto it = ctx.snapshot.roles.find(hub_id);
    if (it == ctx.snapshot.roles.end()) return false;

    Role role = it->second;
    return role == Role::OWNER;
}

void DeleteHubCommand::execute(CommandContext& ctx) {
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

    if (!ctx.snapshot.hubs.count(*internal_hub)) {
        if (!db_.hubs().isHubMember(*internal_hub, ctx.user_id)) {
            output.success = false;
            output.error_code = "not_in_hub";
            output.error_message = "Join the hub before deleting it.";
            json err = {{"type", "error"},
                        {"code", "not_in_hub"},
                        {"message", "Join the hub before deleting it"}};
            DirectMessage msg;
            msg.conn_id = ctx.conn_id;
            msg.payload = err.dump();
            ctx.output.messages.push_back(std::move(msg));
            output.sent_at = std::chrono::system_clock::now();
            return;
        }
        ctx.snapshot.hubs.insert(*internal_hub);
    }

    if (!is_owner(ctx, *internal_hub)) {
        output.success = false;
        output.error_code = "insufficient_privilege";
        output.error_message = "Only owners can delete hubs.";
        json err = {{"type", "error"},
                    {"code", "insufficient_privilege"},
                    {"message", "Only owners can delete hubs"}};
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
        std::unordered_set<ChannelId> hub_channel_ids;
        hub_channel_ids.reserve(channels.size());
        for (const auto& channel : channels) {
            hub_channel_ids.insert(channel.channel_id);
        }

        if (!db_.hubs().deleteHub(*internal_hub, ctx.user_id)) {
            output.success = false;
            output.error_code = "delete_hub_failed";
            output.error_message = "Unable to delete hub.";
            json err = {{"type", "error"},
                        {"code", "delete_hub_failed"},
                        {"message", "Unable to delete hub"}};
            DirectMessage msg;
            msg.conn_id = ctx.conn_id;
            msg.payload = err.dump();
            ctx.output.messages.push_back(std::move(msg));
            output.sent_at = std::chrono::system_clock::now();
            return;
        }

        for (const auto& channel_id : hub_channel_ids) {
            const auto subs = gateway_.subscribers(channel_topic(channel_id));
            const auto public_channel_id = ids_.to_public(channel_id);
            json channel_closed_msg = {{"type", "channel_closed"},
                                       {"channel_id", public_channel_id.value},
                                       {"reason", "hub_deleted"}};
            json hub_deleted_msg = {{"type", "hub_deleted"}, {"hub_id", public_hub_id.value}};

            for (const auto& cid : subs) {
                DirectMessage ch_closed, hub_deleted;
                ch_closed.conn_id = cid;
                ch_closed.payload = channel_closed_msg.dump();
                ctx.output.messages.push_back(std::move(ch_closed));

                hub_deleted.conn_id = cid;
                hub_deleted.payload = hub_deleted_msg.dump();
                hub_deleted.apply_psd = [internal_hub](net::PerSocketData* psd) {
                    if (psd) {
                        auto snapshot = *psd->snapshot;
                        snapshot.hubs.erase(*internal_hub);
                        snapshot.roles.erase(*internal_hub);
                        psd->snapshot = std::make_shared<net::Snapshot>(snapshot);
                    }
                };
                ctx.output.messages.push_back(std::move(hub_deleted));
            }

            gateway_.drop_topic(channel_topic(channel_id));
        }

        output.success = true;
        output.error_code.clear();
        output.error_message.clear();
        output.sent_at = std::chrono::system_clock::now();
    } catch (const std::exception& ex) {
        output.success = false;
        output.error_code = "delete_hub_failed";
        output.error_message = ex.what();
        json err = {{"type", "error"}, {"code", "delete_hub_failed"}, {"message", ex.what()}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
    }
}

}  // namespace app
