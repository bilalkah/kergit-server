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

bool DeleteChannelCommand::has_privilege(net::PerSocketData& psd, const HubId& hub_id) {
    auto it = psd.hub_roles.find(hub_id);
    Role role = Role::USER;
    if (it != psd.hub_roles.end()) {
        role = it->second;
    } else {
        auto db_role = db_.hubs().getMembershipRole(hub_id, psd.user_id);
        if (db_role.has_value()) {
            role = *db_role;
            psd.hub_roles[hub_id] = role;
        }
    }
    return role == Role::OWNER || role == Role::ADMIN;
}

void DeleteChannelCommand::execute(CommandContext& ctx) {
    auto& psd = ctx.psd;
    auto& output = ctx.output;
    const auto& data = ctx.input.data;

    if (!psd.authenticated) {
        output.success = false;
        output.error_code = "not_authenticated";
        output.error_message = "Authenticate before deleting channels.";
        output.data = {{"type", "error"},
                       {"code", "not_authenticated"},
                       {"message", "Authentication required"}};
        return;
    }

    const std::string channel_id_str = data.value("channel_id", "");
    if (channel_id_str.empty()) {
        output.success = false;
        output.error_code = "missing_channel_id";
        output.error_message = "Channel id is required.";
        output.data = {{"type", "error"},
                       {"code", "missing_channel_id"},
                       {"message", "channel_id is required"}};
        return;
    }

    auto internal_channel = ids_.to_internal(PublicChannelId{channel_id_str});
    if (!internal_channel.has_value()) {
        output.success = false;
        output.error_code = "channel_not_found";
        output.error_message = "Channel not found.";
        output.data = {{"type", "error"},
                       {"code", "channel_not_found"},
                       {"message", "Channel does not exist"}};
        return;
    }

    auto channel = db_.channels().getChannel(*internal_channel);
    if (!channel.has_value()) {
        output.success = false;
        output.error_code = "channel_not_found";
        output.error_message = "Channel not found.";
        output.data = {{"type", "error"},
                       {"code", "channel_not_found"},
                       {"message", "Channel does not exist"}};
        return;
    }

    const HubId hub_id = channel->hub_id;
    if (!has_privilege(psd, hub_id)) {
        output.success = false;
        output.error_code = "insufficient_privilege";
        output.error_message = "Only owners or admins can delete channels.";
        output.data = {{"type", "error"},
                       {"code", "insufficient_privilege"},
                       {"message", "Only owners or admins can delete channels"}};
        return;
    }

    try {
        if (!db_.channels().deleteChannel(channel->channel_id, hub_id)) {
            output.success = false;
            output.error_code = "channel_not_found";
            output.error_message = "Channel not found.";
            output.data = {{"type", "error"},
                           {"code", "channel_not_found"},
                           {"message", "Channel does not exist"}};
            return;
        }

        const auto public_hub_id = ids_.to_public(hub_id);
        const auto public_channel_id = ids_.to_public(channel->channel_id);
        json channel_deleted_msg = {{"type", "channel_deleted"},
                                    {"hub_id", public_hub_id.value},
                                    {"channel_id", public_channel_id.value}};
        json channel_closed_msg = {{"type", "channel_closed"},
                                   {"channel_id", public_channel_id.value}};

        struct PendingNotification {
            ConnId conn_id;
            bool unsubscribe{false};
            bool was_current{false};
        };
        std::vector<PendingNotification> pending;
        pending.reserve(32);

        connections_.for_each([&](UwsSocket* ws) {
            if (!ws) return;
            auto* other = ws->getUserData();
            if (!other || !other->authenticated) return;
            if (other->hub_memberships.find(hub_id) == other->hub_memberships.end()) return;

            PendingNotification note{other->conn_id};
            if (other->channel_subscriptions.erase(channel->channel_id) > 0) {
                note.unsubscribe = true;
            }
            if (other->current_channel_id == channel->channel_id) {
                other->current_channel_id = ChannelId{""};
                note.was_current = true;
            }
            pending.push_back(note);
        });

        for (const auto& note : pending) {
            gateway_.send_now(note.conn_id, channel_deleted_msg);
            if (note.unsubscribe) {
                gateway_.unsubscribe(note.conn_id, channel_topic(channel->channel_id));
            }
            if (note.was_current) {
                gateway_.send_now(note.conn_id, channel_closed_msg);
            }
        }

        hub_publisher_.publish_hub(hub_id);

        output.success = true;
        output.error_code.clear();
        output.error_message.clear();
        output.data = {{"type", "channel_deleted"},
                       {"hub_id", public_hub_id.value},
                       {"channel_id", public_channel_id.value}};
        output.sent_at = std::chrono::system_clock::now();
    } catch (const std::exception& ex) {
        output.success = false;
        output.error_code = "delete_failed";
        output.error_message = ex.what();
        output.data = {{"type", "error"},
                       {"code", "delete_failed"},
                       {"message", ex.what()}};
    }
}

}  // namespace app
