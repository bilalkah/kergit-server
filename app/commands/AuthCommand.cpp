#include "app/commands/AuthCommand.h"

#include "infra/persistence/chatdb.h"
#include "net/ClientGateway.h"
#include "net/ConnectionManager.h"
#include "net/PerSocketData.h"

#include "app/services/HubPublisher.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <exception>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace app::services;
using nlohmann::json;

namespace app {

namespace {
std::string safe_display(const net::PerSocketData& psd) {
    if (!psd.username.empty()) return psd.username;
    return "Member";
}

nlohmann::json make_member_payload(const net::PerSocketData& psd) {
    const auto name = safe_display(psd);
    return nlohmann::json{{"handle", name}, {"display_name", name}, {"online", true}};
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

AuthCommand::AuthCommand(ChatDB& db, net::ClientGateway& gateway, net::ConnectionManager& connections,
                         app::services::HubPublisher& hub_publisher)
    : db_(db), gateway_(gateway), connections_(connections), hub_publisher_(hub_publisher) {}

void AuthCommand::execute(CommandContext& ctx) {
    const auto& input = ctx.input;
    auto& psd = ctx.psd;
    auto& output = ctx.output;

    const std::string token = input.data.value("token", "");
    std::string preferred_username;
    if (auto it = input.data.find("username"); it != input.data.end() && it->is_string()) {
        preferred_username = it->get<std::string>();
    }

    auto trim = [](std::string& s) {
        auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
        s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    };
    trim(preferred_username);

    if (token.empty()) {
        output.success = false;
        output.error_code = "missing_token";
        output.data = {{"type", "auth_response"},
                       {"success", false},
                       {"error", "missing_token"},
                       {"error_message", "Authentication token is required"}};
        return;
    }

    auto auth_result = auth_service_.authenticate(token);

    if (!auth_result.success || auth_result.claims.id.empty()) {
        output.success = false;
        output.error_code = "auth_failed";
        output.data = {{"type", "auth_response"},
                       {"success", false},
                       {"error", "auth_failed"},
                       {"error_message", auth_result.error_message.empty() ? "Authentication failed"
                                                                           : auth_result.error_message}};
        return;
    }

    const auto& claims = auth_result.claims;

    try {
        fill_psd(psd, claims);
        if (!preferred_username.empty()) {
            psd.username = preferred_username;
        }
        if (psd.username.empty()) {
            psd.username = "Member";
        }

        // Gather hub & channel data
        const std::vector<Hub> hubs = db_.getUserHubs(psd.user_id);
        std::unordered_map<HubId, std::vector<Channel>> channels_by_hub;
        channels_by_hub.reserve(hubs.size());

        psd.hub_memberships.clear();
        psd.hub_roles.clear();
        for (const auto& hub : hubs) {
            psd.hub_memberships.insert(hub.id);
            Role role = Role::USER;
            auto it = hub.members.find(psd.user_id);
            if (it != hub.members.end()) role = it->second;
            psd.hub_roles[hub.id] = role;
            channels_by_hub.emplace(hub.id, db_.getHubChannels(hub.id));
        }

        auto online_by_hub = collect_online_members(hubs);
        subscribe_to_hubs(psd, hubs);

        auto bootstrap = build_bootstrap_payload(hubs, channels_by_hub, online_by_hub, psd.user_id);
        gateway_.send_now(psd.conn_id, bootstrap);
        if (!psd.hub_memberships.empty()) {
            hub_publisher_.publish_hubs(psd.hub_memberships);
        }

        output.success = true;
        output.error_code.clear();
        output.error_message.clear();
        output.data = {{"type", "auth_response"},
                       {"success", true},
                       {"user_id", psd.user_id.value}};
        if (!psd.username.empty()) output.data["username"] = psd.username;
        output.sent_at = std::chrono::system_clock::now();
    } catch (const std::exception& ex) {
        psd.authenticated = false;
        psd.hub_memberships.clear();
        psd.hub_roles.clear();
        output.success = false;
        output.error_code = "bootstrap_failed";
        output.error_message = ex.what();
        output.data = {{"type", "auth_response"},
                       {"success", false},
                       {"error", "bootstrap_failed"},
                       {"error_message", ex.what()}};
    }
}

void AuthCommand::fill_psd(net::PerSocketData& psd,
                           const infra::security::token::UserClaims& claims) {
    psd.user_id = UserId{claims.id};
    psd.authenticated = true;
    psd.authenticated_at = std::chrono::system_clock::now();
    psd.email = claims.email;

    psd.username.clear();
    if (!claims.username.empty()) {
        psd.username = claims.username;
    } else if (!claims.full_name.empty()) {
        psd.username = claims.full_name;
    }
}

void AuthCommand::subscribe_to_hubs(const net::PerSocketData& psd,
                                    const std::vector<Hub>& hubs) const {
    for (const auto& hub : hubs) {
        if (!hub.id.value.empty()) {
            gateway_.subscribe(psd.conn_id, HubPublisher::topic_for(hub.id));
        }
    }
}

nlohmann::json AuthCommand::collect_online_members(const std::vector<Hub>& hubs) const {
    std::unordered_set<HubId> target_hubs;
    target_hubs.reserve(hubs.size());
    for (const auto& hub : hubs) {
        if (!hub.id.value.empty()) target_hubs.insert(hub.id);
    }

    std::unordered_map<HubId, nlohmann::json> per_hub_members;
    connections_.for_each([&](UwsSocket* ws) {
        if (!ws) return;
        auto* other_psd = ws->getUserData();
        if (!other_psd || !other_psd->authenticated) return;
        if (other_psd->user_id.value.empty()) return;

        nlohmann::json member = make_member_payload(*other_psd);

        for (const auto& hub_id : other_psd->hub_memberships) {
            if (!target_hubs.empty() && !target_hubs.count(hub_id)) continue;
            auto [it, inserted] = per_hub_members.try_emplace(hub_id, nlohmann::json::array());
            if (!it->second.is_array()) it->second = nlohmann::json::array();
            it->second.push_back(member);
        }
    });

    nlohmann::json result = nlohmann::json::object();
    for (const auto& hub : hubs) {
        const auto& hub_id = hub.id;
        auto it = per_hub_members.find(hub_id);
        if (it != per_hub_members.end())
            result[hub_id.value] = it->second;
        else
            result[hub_id.value] = nlohmann::json::array();
    }
    return result;
}

nlohmann::json AuthCommand::build_bootstrap_payload(
    const std::vector<Hub>& hubs,
    const std::unordered_map<HubId, std::vector<Channel>>& channels_by_hub,
    const nlohmann::json& online_by_hub, const UserId& current_user) const {
    nlohmann::json hubs_json = nlohmann::json::array();
    for (const auto& hub : hubs) {
        nlohmann::json hub_json = {{"id", hub.id.value}, {"name", hub.name}};
        auto it = hub.members.find(current_user);
        if (it != hub.members.end()) hub_json["role"] = role_to_string(it->second);
        hubs_json.push_back(std::move(hub_json));
    }

    nlohmann::json channels_json = nlohmann::json::object();
    for (const auto& hub : hubs) {
        nlohmann::json arr = nlohmann::json::array();
        auto it = channels_by_hub.find(hub.id);
        if (it != channels_by_hub.end()) {
            for (const auto& channel : it->second) {
                arr.push_back({{"id", channel.channel_id.value},
                               {"hub_id", channel.hub_id.value},
                               {"name", channel.name},
                               {"type", channel_type_to_string(channel.type)}});
            }
        }
        channels_json[hub.id.value] = std::move(arr);
    }

    return nlohmann::json{
        {"type", "hubs_list"},
        {"hubs", std::move(hubs_json)},
        {"channels_by_hub", std::move(channels_json)},
        {"online_by_hub", online_by_hub},
    };
}

}  // namespace app
