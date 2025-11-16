#include "app/commands/JoinHubByInviteCommand.h"

#include "app/services/HubPublisher.h"
#include "app/services/PublicIdService.h"
#include "infra/persistence/PersistenceGateway.h"
#include "net/ClientGateway.h"
#include "net/ConnectionManager.h"
#include "net/PerSocketData.h"

#include <nlohmann/json.hpp>
#include <string>
#include <unordered_set>

using nlohmann::json;

namespace app {

JoinHubByInviteCommand::JoinHubByInviteCommand(PersistenceGateway& db, net::ClientGateway& gateway,
                                               net::ConnectionManager& connections,
                                               app::services::HubPublisher& hub_publisher,
                                               app::services::PublicIdService& ids)
    : db_(db),
      gateway_(gateway),
      connections_(connections),
      hub_publisher_(hub_publisher),
      ids_(ids) {}

json JoinHubByInviteCommand::build_members_payload(const HubId& hub_id) {
    json members = json::array();
    auto db_members = db_.hubs().getHubMembers(hub_id);
    std::unordered_set<UserId> online_members;
    connections_.for_each([&](UwsSocket* ws) {
        if (!ws) return;
        auto* other = ws->getUserData();
        if (!other || !other->authenticated) return;
        if (other->hub_memberships.find(hub_id) == other->hub_memberships.end()) return;
        online_members.insert(other->user_id);
    });

    for (const auto& [user_id, display_hint] : db_members) {
        std::string display = ids_.display_for(user_id);
        if (display.empty() && !display_hint.empty()) {
            display = display_hint;
            ids_.remember_display(user_id, display);
        }
        if (display.empty()) {
            if (auto db_name = db_.users().getUserDisplayName(user_id)) {
                if (!db_name->empty()) {
                    display = *db_name;
                    ids_.remember_display(user_id, display);
                }
            }
        }
        if (display.empty()) display = "Member";
        const auto public_user = ids_.to_public(user_id);
        members.push_back({{"handle", display},
                           {"display_name", display},
                           {"online", online_members.count(user_id) > 0},
                           {"user_id", public_user.value}});
    }
    return members;
}

json JoinHubByInviteCommand::build_channels_payload(const HubId& hub_id) {
    json channels_json = json::array();
    const auto channels = db_.channels().getHubChannels(hub_id);
    for (const auto& channel : channels) {
        const auto public_channel_id = ids_.to_public(channel.channel_id);
        const auto public_hub_id = ids_.to_public(channel.hub_id);
        std::string type = channel.type == ChannelType::VOICE ? "voice" : "text";
        channels_json.push_back({{"id", public_channel_id.value},
                                 {"hub_id", public_hub_id.value},
                                 {"name", channel.name},
                                 {"type", type}});
    }
    return channels_json;
}

void JoinHubByInviteCommand::execute(CommandContext& ctx) {
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

    const std::string invite_code = data.value("invite_code", "");
    if (invite_code.empty()) {
        output.success = false;
        output.error_code = "invalid_invite";
        output.error_message = "Invite code is required.";
        output.data = {
            {"type", "error"}, {"code", "invalid_invite"}, {"message", "Invite code is required"}};
        return;
    }

    auto internal_hub = ids_.to_internal(PublicHubId{invite_code});
    if (!internal_hub.has_value()) {
        output.success = false;
        output.error_code = "invite_not_found";
        output.error_message = "Invite code is invalid.";
        output.data = {
            {"type", "error"}, {"code", "invite_not_found"}, {"message", "Invite code is invalid"}};
        return;
    }

    auto hub_opt = db_.hubs().getHub(*internal_hub);
    if (!hub_opt.has_value()) {
        output.success = false;
        output.error_code = "hub_not_found";
        output.error_message = "Hub not found.";
        output.data = {{"type", "error"}, {"code", "hub_not_found"}, {"message", "Hub not found"}};
        return;
    }

    if (psd.hub_memberships.count(*internal_hub) ||
        db_.hubs().isHubMember(*internal_hub, psd.user_id)) {
        output.success = true;
        output.error_code.clear();
        output.error_message.clear();
        const auto public_hub_id = ids_.to_public(*internal_hub);
        json hub_json = {{"id", public_hub_id.value}, {"name", hub_opt->name}, {"role", "member"}};
        output.data = {{"type", "hub_joined"},
                       {"hub", std::move(hub_json)},
                       {"channels", build_channels_payload(*internal_hub)},
                       {"members", build_members_payload(*internal_hub)},
                       {"already_member", true}};
        output.sent_at = std::chrono::system_clock::now();
        return;
    }

    try {
        db_.hubs().addMember(*internal_hub, psd.user_id, "member");
        psd.hub_memberships.insert(*internal_hub);
        psd.hub_roles[*internal_hub] = Role::USER;

        gateway_.subscribe(psd.conn_id, app::services::HubPublisher::topic_for(*internal_hub));

        const auto public_hub_id = ids_.to_public(*internal_hub);
        json hub_json = {
            {"id", public_hub_id.value},
            {"name", hub_opt->name},
            {"role", "member"},
        };

        hub_publisher_.publish_hub(*internal_hub);

        output.success = true;
        output.error_code.clear();
        output.error_message.clear();
        output.data = {{"type", "hub_joined"},
                       {"hub", std::move(hub_json)},
                       {"channels", build_channels_payload(*internal_hub)},
                       {"members", build_members_payload(*internal_hub)},
                       {"already_member", false}};
        output.sent_at = std::chrono::system_clock::now();
    } catch (const std::exception& ex) {
        output.success = false;
        output.error_code = "join_hub_failed";
        output.error_message = ex.what();
        output.data = {{"type", "error"}, {"code", "join_hub_failed"}, {"message", ex.what()}};
    }
}

}  // namespace app
