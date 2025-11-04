#include "app/commands/ListCommand.h"

#include "app/services/HubPublisher.h"
#include "infra/persistence/chatdb.h"
#include "net/ConnectionManager.h"
#include "net/PerSocketData.h"

#include <nlohmann/json.hpp>

#include <exception>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using nlohmann::json;

namespace app {

namespace {
std::string safe_display(const net::PerSocketData& psd) {
    if (!psd.username.empty()) return psd.username;
    return "Member";
}

json make_member_payload(const net::PerSocketData& psd) {
    const auto name = safe_display(psd);
    return json{{"handle", name}, {"display_name", name}, {"online", true}};
}

std::string role_to_string(Role role) {
    switch (role) {
        case Role::OWNER:
            return "owner";
        case Role::ADMIN:
            return "admin";
        default:
            return "member";
    }
}

std::string channel_type_to_string(ChannelType type) {
    return type == ChannelType::VOICE ? "voice" : "text";
}
}  // namespace

ListCommand::ListCommand(ChatDB& db, net::ConnectionManager& connections,
                         app::services::HubPublisher& hub_publisher)
    : db_(db), connections_(connections), hub_publisher_(hub_publisher) {}

void ListCommand::execute(CommandContext& ctx) {
    auto& psd = ctx.psd;
    auto& output = ctx.output;

    if (!psd.authenticated) {
        output.success = false;
        output.error_code = "not_authenticated";
        output.error_message = "Authenticate before requesting data.";
        output.data = {{"type", "error"},
                       {"code", "not_authenticated"},
                       {"message", "Authentication required"}};
        return;
    }

    try {
        const auto hubs = db_.getUserHubs(psd.user_id);
        std::unordered_map<HubId, std::vector<Channel>> channels_by_hub;
        channels_by_hub.reserve(hubs.size());
        for (const auto& hub : hubs) {
            channels_by_hub.emplace(hub.id, db_.getHubChannels(hub.id));
        }

        const auto online_by_hub = collect_online_members(hubs);
        auto payload = build_payload(hubs, channels_by_hub, online_by_hub, psd.user_id);

        output.success = true;
        output.error_code.clear();
        output.error_message.clear();
        output.data = std::move(payload);
        output.sent_at = std::chrono::system_clock::now();

        if (!hubs.empty()) {
            std::unordered_set<HubId> ids;
            ids.reserve(hubs.size());
            for (const auto& hub : hubs) ids.insert(hub.id);
            hub_publisher_.publish_hubs(ids);
        }
    } catch (const std::exception& ex) {
        output.success = false;
        output.error_code = "list_failed";
        output.error_message = ex.what();
        output.data = {{"type", "error"},
                       {"code", "list_failed"},
                       {"message", ex.what()}};
    }
}

json ListCommand::build_payload(const std::vector<Hub>& hubs,
                                const std::unordered_map<HubId, std::vector<Channel>>& channels,
                                const json& online_by_hub, const UserId& current_user) const {
    json hubs_json = json::array();
    for (const auto& hub : hubs) {
        json hub_json = {{"id", hub.id.value}, {"name", hub.name}};
        auto it = hub.members.find(current_user);
        if (it != hub.members.end()) hub_json["role"] = role_to_string(it->second);
        hubs_json.push_back(std::move(hub_json));
    }

    json channels_json = json::object();
    for (const auto& hub : hubs) {
        json arr = json::array();
        auto it = channels.find(hub.id);
        if (it != channels.end()) {
            for (const auto& channel : it->second) {
                arr.push_back({{"id", channel.channel_id.value},
                               {"hub_id", channel.hub_id.value},
                               {"name", channel.name},
                               {"type", channel_type_to_string(channel.type)}});
            }
        }
        channels_json[hub.id.value] = std::move(arr);
    }

    return {{"type", "hubs_list"},
            {"hubs", std::move(hubs_json)},
            {"channels_by_hub", std::move(channels_json)},
            {"online_by_hub", online_by_hub}};
}

json ListCommand::collect_online_members(const std::vector<Hub>& hubs) const {
    std::unordered_set<HubId> target;
    target.reserve(hubs.size());
    for (const auto& hub : hubs) {
        if (!hub.id.value.empty()) target.insert(hub.id);
    }

    std::unordered_map<HubId, json> per_hub;
    connections_.for_each([&](UwsSocket* ws) {
        if (!ws) return;
        auto* other = ws->getUserData();
        if (!other || !other->authenticated) return;
        if (other->user_id.value.empty()) return;

        json member = make_member_payload(*other);
        for (const auto& hub_id : other->hub_memberships) {
            if (!target.empty() && !target.count(hub_id)) continue;
            auto [it, inserted] = per_hub.try_emplace(hub_id, json::array());
            if (!it->second.is_array()) it->second = json::array();
            it->second.push_back(member);
        }
    });

    json result = json::object();
    for (const auto& hub : hubs) {
        auto it = per_hub.find(hub.id);
        if (it != per_hub.end())
            result[hub.id.value] = it->second;
        else
            result[hub.id.value] = json::array();
    }
    return result;
}

}  // namespace app

