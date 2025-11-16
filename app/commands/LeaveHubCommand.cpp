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
namespace {
std::string display_from_psd(const net::PerSocketData& psd) {
    if (!psd.username.empty()) return psd.username;
    return "Member";
}
}  // namespace

LeaveHubCommand::LeaveHubCommand(PersistenceGateway& db, net::ClientGateway& gateway,
                                 net::ConnectionManager& connections,
                                 app::services::HubPublisher& hub_publisher,
                                 app::services::PublicIdService& ids)
    : db_(db),
      gateway_(gateway),
      connections_(connections),
      hub_publisher_(hub_publisher),
      ids_(ids) {}

bool LeaveHubCommand::is_owner(net::PerSocketData& psd, const HubId& hub_id) {
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
    return role == Role::OWNER;
}

std::string LeaveHubCommand::channel_topic(const ChannelId& channel_id) {
    return "channel:" + channel_id.value;
}

void LeaveHubCommand::publish_presence_update(const ChannelId& channel_id,
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

void LeaveHubCommand::execute(CommandContext& ctx) {
    auto& psd = ctx.psd;
    auto& output = ctx.output;
    const auto& data = ctx.input.data;

    if (!psd.authenticated) {
        output.success = false;
        output.error_code = "not_authenticated";
        output.error_message = "Authentication required.";
        output.data = {{"type", "error"},
                       {"code", "not_authenticated"},
                       {"message", "Authentication required"}};
        return;
    }

    const std::string hub_id_str = data.value("hub_id", "");
    if (hub_id_str.empty()) {
        output.success = false;
        output.error_code = "missing_hub_id";
        output.error_message = "hub_id is required.";
        output.data = {
            {"type", "error"}, {"code", "missing_hub_id"}, {"message", "hub_id is required"}};
        return;
    }

    auto internal_hub = ids_.to_internal(PublicHubId{hub_id_str});
    if (!internal_hub.has_value()) {
        output.success = false;
        output.error_code = "hub_not_found";
        output.error_message = "Hub not found.";
        output.data = {
            {"type", "error"}, {"code", "hub_not_found"}, {"message", "Hub does not exist"}};
        return;
    }

    if (!psd.hub_memberships.count(*internal_hub) &&
        !db_.hubs().isHubMember(*internal_hub, psd.user_id)) {
        output.success = false;
        output.error_code = "not_in_hub";
        output.error_message = "Join the hub before leaving it.";
        output.data = {{"type", "error"},
                       {"code", "not_in_hub"},
                       {"message", "Join the hub before leaving it"}};
        return;
    }

    if (is_owner(psd, *internal_hub)) {
        output.success = false;
        output.error_code = "hub_owner_must_transfer";
        output.error_message = "Owners must transfer ownership before leaving.";
        output.data = {{"type", "error"},
                       {"code", "hub_owner_must_transfer"},
                       {"message", "Transfer ownership or delete the hub before leaving"}};
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

        db_.hubs().removeMember(*internal_hub, psd.user_id);

        struct PendingNotification {
            ConnId conn_id;
            bool unsubscribe_hub_topic{false};
            std::vector<ChannelId> channel_ids;
            bool send_hub_left{false};
        };
        std::vector<PendingNotification> pending;
        pending.reserve(4);
        const auto hub_topic = app::services::HubPublisher::topic_for(*internal_hub);

        connections_.for_each([&](UwsSocket* ws) {
            if (!ws) return;
            auto* other = ws->getUserData();
            if (!other || !other->authenticated) return;
            if (other->user_id.value != psd.user_id.value) return;
            if (other->hub_memberships.erase(*internal_hub) == 0) return;

            other->hub_roles.erase(*internal_hub);

            std::vector<ChannelId> to_remove;
            to_remove.reserve(other->channel_subscriptions.size());
            for (const auto& channel_id : other->channel_subscriptions) {
                if (hub_channel_ids.find(channel_id) != hub_channel_ids.end()) {
                    to_remove.push_back(channel_id);
                }
            }
            for (const auto& channel_id : to_remove) {
                other->channel_subscriptions.erase(channel_id);
                publish_presence_update(channel_id, *other, false);
            }

            if (!other->current_channel_id.value.empty() &&
                hub_channel_ids.find(other->current_channel_id) != hub_channel_ids.end()) {
                other->current_channel_id = ChannelId{""};
            }
            if (other->current_hub_id == *internal_hub) {
                other->current_hub_id = HubId{""};
            }

            PendingNotification note;
            note.conn_id = other->conn_id;
            note.unsubscribe_hub_topic = true;
            note.channel_ids = std::move(to_remove);
            note.send_hub_left = other->conn_id.value != psd.conn_id.value;
            pending.push_back(std::move(note));
        });

        for (const auto& note : pending) {
            if (note.unsubscribe_hub_topic) {
                gateway_.unsubscribe(note.conn_id, hub_topic);
            }
            for (const auto& channel_id : note.channel_ids) {
                gateway_.unsubscribe(note.conn_id, channel_topic(channel_id));
            }
            if (note.send_hub_left) {
                gateway_.send_now(note.conn_id,
                                  {{"type", "hub_left"}, {"hub_id", public_hub_id.value}});
            }
        }

        hub_publisher_.publish_hub(*internal_hub);

        output.success = true;
        output.error_code.clear();
        output.error_message.clear();
        output.data = {{"type", "hub_left"}, {"hub_id", public_hub_id.value}};
        output.sent_at = std::chrono::system_clock::now();
    } catch (const std::exception& ex) {
        output.success = false;
        output.error_code = "leave_hub_failed";
        output.error_message = ex.what();
        output.data = {{"type", "error"}, {"code", "leave_hub_failed"}, {"message", ex.what()}};
    }
}

}  // namespace app
