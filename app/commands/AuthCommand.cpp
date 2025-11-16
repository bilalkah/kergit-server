#include "app/commands/AuthCommand.h"

#include "app/services/HubPublisher.h"
#include "infra/persistence/PersistenceGateway.h"
#include "net/ClientGateway.h"
#include "net/ConnectionManager.h"
#include "net/PerSocketData.h"

#include <algorithm>
#include <cctype>
#include <exception>
#include <nlohmann/json.hpp>
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

AuthCommand::AuthCommand(PersistenceGateway& db, net::ClientGateway& gateway,
                         net::ConnectionManager& connections,
                         app::services::HubPublisher& hub_publisher,
                         app::services::PublicIdService& ids)
    : db_(db),
      gateway_(gateway),
      connections_(connections),
      hub_publisher_(hub_publisher),
      ids_(ids) {}

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
        output.data = {
            {"type", "auth_response"},
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
        if (!psd.username.empty()) {
            ids_.remember_display(psd.user_id, psd.username);
        }

        // Gather hub & channel data
        const std::vector<Hub> hubs = db_.hubs().getUserHubs(psd.user_id);
        ids_.to_public(psd.user_id);
        std::unordered_map<HubId, std::vector<Channel>> channels_by_hub;
        channels_by_hub.reserve(hubs.size());

        psd.hub_memberships.clear();
        psd.hub_roles.clear();
        for (const auto& hub : hubs) {
            ids_.to_public(hub.id);
            psd.hub_memberships.insert(hub.id);
            Role role = Role::USER;
            auto it = hub.members.find(psd.user_id);
            if (it != hub.members.end()) role = it->second;
            psd.hub_roles[hub.id] = role;
            auto channels = db_.channels().getHubChannels(hub.id);
            for (const auto& channel : channels) {
                ids_.to_public(channel.channel_id);
                ids_.to_public(channel.hub_id);
            }
            channels_by_hub.emplace(hub.id, std::move(channels));
        }

        auto online_by_hub = collect_online_members(hubs);
        subscribe_to_hubs(psd, hubs);

        auto bootstrap = build_bootstrap_payload(hubs, channels_by_hub, online_by_hub, psd.user_id);
        gateway_.send_now(psd.conn_id, bootstrap);
        if (!psd.hub_memberships.empty()) {
            hub_publisher_.publish_hubs(psd.hub_memberships);
        }

        const auto public_user_id = ids_.to_public(psd.user_id);

        output.success = true;
        output.error_code.clear();
        output.error_message.clear();
        output.data = {
            {"type", "auth_response"}, {"success", true}, {"user_id", public_user_id.value}};
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
    } else {
        if (auto db_name = db_.users().getUserDisplayName(psd.user_id)) {
            psd.username = *db_name;
        }
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

nlohmann::json AuthCommand::collect_online_members(const std::vector<Hub>& hubs) {
    std::unordered_set<HubId> target_hubs;
    target_hubs.reserve(hubs.size());
    for (const auto& hub : hubs) {
        if (!hub.id.value.empty()) target_hubs.insert(hub.id);
    }

    std::unordered_map<HubId, std::unordered_map<UserId, std::string>> online_by_hub;
    connections_.for_each([&](UwsSocket* ws) {
        if (!ws) return;
        auto* other_psd = ws->getUserData();
        if (!other_psd || !other_psd->authenticated) return;
        if (other_psd->user_id.value.empty()) return;

        const auto display = safe_display(*other_psd);
        if (!display.empty()) ids_.remember_display(other_psd->user_id, display);

        for (const auto& hub_id : other_psd->hub_memberships) {
            if (!target_hubs.empty() && !target_hubs.count(hub_id)) continue;
            ids_.to_public(hub_id);
            online_by_hub[hub_id][other_psd->user_id] = display;
        }
    });

    nlohmann::json result = nlohmann::json::object();
    for (const auto& hub : hubs) {
        const auto public_id = ids_.to_public(hub.id);
        const auto members = db_.hubs().getHubMembers(hub.id);
        nlohmann::json arr = nlohmann::json::array();
        std::unordered_set<UserId> seen;
        auto it_online = online_by_hub.find(hub.id);

        for (const auto& [member_id, stored_display] : members) {
            const bool is_online = it_online != online_by_hub.end() &&
                                   it_online->second.find(member_id) != it_online->second.end();
            if (!stored_display.empty()) ids_.remember_display(member_id, stored_display);
            std::string display = ids_.display_for(member_id);
            if (display.empty() && it_online != online_by_hub.end()) {
                auto it_display = it_online->second.find(member_id);
                if (it_display != it_online->second.end()) display = it_display->second;
            }
            if (display.empty() && !stored_display.empty()) display = stored_display;
            if (display.empty()) {
                if (auto db_name = db_.users().getUserDisplayName(member_id)) {
                    if (!db_name->empty()) {
                        display = *db_name;
                        ids_.remember_display(member_id, display);
                    }
                }
            }
            if (display.empty()) display = "Member";
            const auto public_user = ids_.to_public(member_id);
            arr.push_back({{"handle", display},
                           {"display_name", display},
                           {"online", is_online},
                           {"user_id", public_user.value}});
            seen.insert(member_id);
        }

        if (it_online != online_by_hub.end()) {
            for (const auto& [user_id, display] : it_online->second) {
                if (seen.find(user_id) != seen.end()) continue;
                const auto public_user = ids_.to_public(user_id);
                std::string name = display.empty() ? ids_.display_for(user_id) : display;
                if (name.empty()) {
                    if (auto db_name = db_.users().getUserDisplayName(user_id)) {
                        if (!db_name->empty()) {
                            name = *db_name;
                            ids_.remember_display(user_id, name);
                        }
                    }
                }
                if (name.empty()) name = "Member";
                arr.push_back({{"handle", name},
                               {"display_name", name},
                               {"online", true},
                               {"user_id", public_user.value}});
            }
        }

        result[public_id.value] = std::move(arr);
    }
    return result;
}

nlohmann::json AuthCommand::build_bootstrap_payload(
    const std::vector<Hub>& hubs,
    const std::unordered_map<HubId, std::vector<Channel>>& channels_by_hub,
    const nlohmann::json& online_by_hub, const UserId& current_user) {
    nlohmann::json hubs_json = nlohmann::json::array();
    for (const auto& hub : hubs) {
        const auto public_hub_id = ids_.to_public(hub.id);
        nlohmann::json hub_json = {{"id", public_hub_id.value}, {"name", hub.name}};
        auto it = hub.members.find(current_user);
        if (it != hub.members.end()) hub_json["role"] = role_to_string(it->second);
        hubs_json.push_back(std::move(hub_json));
    }

    nlohmann::json channels_json = nlohmann::json::object();
    for (const auto& hub : hubs) {
        const auto public_hub_id = ids_.to_public(hub.id);
        nlohmann::json arr = nlohmann::json::array();
        auto it = channels_by_hub.find(hub.id);
        if (it != channels_by_hub.end()) {
            for (const auto& channel : it->second) {
                const auto public_channel_id = ids_.to_public(channel.channel_id);
                arr.push_back({{"id", public_channel_id.value},
                               {"hub_id", public_hub_id.value},
                               {"name", channel.name},
                               {"type", channel_type_to_string(channel.type)}});
            }
        }
        channels_json[public_hub_id.value] = std::move(arr);
    }

    return nlohmann::json{
        {"type", "hubs_list"},
        {"hubs", std::move(hubs_json)},
        {"channels_by_hub", std::move(channels_json)},
        {"online_by_hub", online_by_hub},
    };
}

}  // namespace app
