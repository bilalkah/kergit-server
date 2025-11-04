#include "app/commands/AuthCommand.h"

#include "infra/persistence/chatdb.h"
#include "net/ClientGateway.h"
#include "net/ConnectionManager.h"
#include "net/PerSocketData.h"

#include "app/services/HubPublisher.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <exception>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace app::services;

namespace app {

AuthCommand::AuthCommand(ChatDB& db, net::ClientGateway& gateway, net::ConnectionManager& connections,
                         app::services::HubPublisher& hub_publisher)
    : db_(db), gateway_(gateway), connections_(connections), hub_publisher_(hub_publisher) {}

void AuthCommand::execute(CommandContext& ctx) {
    const auto& input = ctx.input;
    auto& psd = ctx.psd;
    auto& output = ctx.output;

    const std::string token = input.data.value("token", "");

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

        // Gather hub & channel data
        const std::vector<HubInfo> hubs = db_.getUserHubs(psd.user_id);
        std::unordered_map<HubId, std::vector<ChannelInfo>> channels_by_hub;
        channels_by_hub.reserve(hubs.size());

        psd.hub_memberships.clear();
        for (const auto& hub : hubs) {
            psd.hub_memberships.insert(hub.id);
            channels_by_hub.emplace(hub.id, db_.getHubChannels(hub.id));
        }

        auto online_by_hub = collect_online_members(hubs);
        subscribe_to_hubs(psd, hubs);

        auto bootstrap = build_bootstrap_payload(hubs, channels_by_hub, online_by_hub);
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
        if (!psd.email.empty()) output.data["email"] = psd.email;
        output.sent_at = std::chrono::system_clock::now();
    } catch (const std::exception& ex) {
        psd.authenticated = false;
        psd.hub_memberships.clear();
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

    if (!claims.username.empty()) {
        psd.username = claims.username;
    } else if (!claims.full_name.empty()) {
        psd.username = claims.full_name;
    } else if (!claims.email.empty()) {
        auto at_pos = claims.email.find('@');
        psd.username = at_pos == std::string::npos ? claims.email : claims.email.substr(0, at_pos);
    } else {
        psd.username = claims.id;
    }
}

void AuthCommand::subscribe_to_hubs(const net::PerSocketData& psd,
                                    const std::vector<HubInfo>& hubs) const {
    for (const auto& hub : hubs) {
        if (!hub.id.value.empty()) {
            gateway_.subscribe(psd.conn_id, HubPublisher::topic_for(hub.id));
        }
    }
}

nlohmann::json AuthCommand::collect_online_members(const std::vector<HubInfo>& hubs) const {
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

        nlohmann::json member = {{"user_id", other_psd->user_id.value}};
        if (!other_psd->username.empty()) member["username"] = other_psd->username;
        if (!other_psd->email.empty()) member["email"] = other_psd->email;
        member["online"] = true;

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
    const std::vector<HubInfo>& hubs,
    const std::unordered_map<HubId, std::vector<ChannelInfo>>& channels_by_hub,
    const nlohmann::json& online_by_hub) const {
    nlohmann::json hubs_json = nlohmann::json::array();
    for (const auto& hub : hubs) {
        hubs_json.push_back({{"id", hub.id.value}, {"name", hub.name}});
    }

    nlohmann::json channels_json = nlohmann::json::object();
    for (const auto& hub : hubs) {
        auto it = channels_by_hub.find(hub.id);
        nlohmann::json arr = nlohmann::json::array();
        if (it != channels_by_hub.end()) {
            for (const auto& channel : it->second) {
                arr.push_back({{"id", channel.id.value},
                               {"hub_id", channel.hub_id.value},
                               {"name", channel.name},
                               {"type", channel.type}});
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
