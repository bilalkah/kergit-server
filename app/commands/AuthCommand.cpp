#include "app/commands/AuthCommand.h"

#include "app/queue/OutgoingQueue.h"  // DirectMessage, PublishMessage, OutgoingMessage
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

// NOTE: PSD in worker is not available anymore.
// We only read other sockets' PSDs inside collect_online_members,
// and only WRITE PSD inside apply_psd (WS thread).

std::string safe_display(const net::PerSocketData& psd) {
    if (!psd.username.empty()) return psd.username;
    return "Member";
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
    auto& output = ctx.output;

    const std::string token = input.data.value("token", "");
    std::string preferred_username;
    if (auto it = input.data.find("username"); it != input.data.end() && it->is_string()) {
        preferred_username = it->get<std::string>();
    }

    // trim username
    auto trim = [](std::string& s) {
        auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
        s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    };
    trim(preferred_username);

    // -------------------------
    // Validate token
    // -------------------------
    if (token.empty()) {
        output.success = false;
        output.error_code = "missing_token";
        output.error_message = "Authentication token is required";

        json err = {{"type", "auth_response"},
                    {"success", false},
                    {"error", "missing_token"},
                    {"error_message", "Authentication token is required"}};

        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        return;
    }

    auto auth_result = auth_service_.authenticate(token);

    if (!auth_result.success || auth_result.claims.id.empty()) {
        output.success = false;
        output.error_code = "auth_failed";
        output.error_message =
            auth_result.error_message.empty() ? "Authentication failed" : auth_result.error_message;

        json err = {{"type", "auth_response"},
                    {"success", false},
                    {"error", "auth_failed"},
                    {"error_message", output.error_message}};

        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        return;
    }

    // -------------------------
    // Auth success -> bootstrap
    // -------------------------
    const auto& claims = auth_result.claims;

    try {
        // Build worker-side user fields
        UserId user_id{claims.id};
        std::string email = claims.email;
        std::string username;

        if (!preferred_username.empty()) {
            username = preferred_username;
        } else if (!claims.username.empty()) {
            username = claims.username;
        } else if (!claims.full_name.empty()) {
            username = claims.full_name;
        } else {
            if (auto db_name = db_.users().getUserDisplayName(user_id)) {
                username = *db_name;
            }
        }
        if (username.empty()) username = "Member";

        ids_.remember_display(user_id, username);
        ids_.to_public(user_id);

        // Gather hubs & channels
        const std::vector<Hub> hubs = db_.hubs().getUserHubs(user_id);

        std::unordered_map<HubId, std::vector<Channel>> channels_by_hub;
        channels_by_hub.reserve(hubs.size());

        // Build snapshot for this user/connection
        auto snapshot = std::make_shared<net::Snapshot>();
        snapshot->hubs.clear();
        snapshot->roles.clear();
        snapshot->channels.clear();

        for (const auto& hub : hubs) {
            ids_.to_public(hub.id);
            snapshot->hubs.insert(hub.id);

            Role role = Role::USER;
            auto it = hub.members.find(user_id);
            if (it != hub.members.end()) role = it->second;
            snapshot->roles[hub.id] = role;

            auto channels = db_.channels().getHubChannels(hub.id);
            for (const auto& channel : channels) {
                ids_.to_public(channel.channel_id);
                ids_.to_public(channel.hub_id);
                snapshot->channels.insert(channel.channel_id);
            }
            channels_by_hub.emplace(hub.id, std::move(channels));
        }

        // Online members by hub (reads other PSD snapshots)
        auto online_by_hub = collect_online_members(hubs);

        // Bootstrap payload
        auto bootstrap = build_bootstrap_payload(hubs, channels_by_hub, online_by_hub, user_id);

        // 1) Send bootstrap list to this client
        {
            DirectMessage boot_msg;
            boot_msg.conn_id = ctx.conn_id;
            boot_msg.payload = bootstrap.dump();
            ctx.output.messages.push_back(std::move(boot_msg));
        }

        // 2) Send auth_response + apply_psd to update PSD & subscribe hubs in WS thread
        const auto public_user_id = ids_.to_public(user_id);

        json auth_resp = {
            {"type", "auth_response"},
            {"success", true},
            {"user_id", public_user_id.value},
            {"username", username},
        };

        DirectMessage auth_msg;
        auth_msg.conn_id = ctx.conn_id;
        auth_msg.payload = auth_resp.dump();

        // Capture what WS thread needs
        auto snap_const = std::shared_ptr<const net::Snapshot>(snapshot);
        auto hubs_copy = snapshot->hubs;

        auth_msg.apply_psd = [this, user_id, email, username, snap_const,
                              hubs_copy](net::PerSocketData* psd) mutable {
            if (!psd) return;

            psd->user_id = user_id;
            psd->email = email;
            psd->username = username;
            psd->authenticated = true;
            psd->alive = true;
            psd->authenticated_at = std::chrono::system_clock::now();
            psd->snapshot = std::move(snap_const);

            // Subscribe to hub topics (WS thread)
            for (const auto& hub_id : hubs_copy) {
                if (!hub_id.value.empty()) {
                    gateway_.subscribe(psd->conn_id, HubPublisher::topic_for(hub_id));
                }
            }

            // Publish hub updates if you want this side-effect on auth
            if (!hubs_copy.empty()) {
                hub_publisher_.publish_hubs(hubs_copy);
            }
        };

        ctx.output.messages.push_back(std::move(auth_msg));

        output.success = true;
        output.error_code.clear();
        output.error_message.clear();
        output.sent_at = std::chrono::system_clock::now();
    } catch (const std::exception& ex) {
        output.success = false;
        output.error_code = "bootstrap_failed";
        output.error_message = ex.what();
        output.sent_at = std::chrono::system_clock::now();

        json err = {{"type", "auth_response"},
                    {"success", false},
                    {"error", "bootstrap_failed"},
                    {"error_message", ex.what()}};

        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
    }
}

// OLD fill_psd is not used anymore since worker cannot mutate PSD.
// You can delete this declaration from header too, but leaving here in case
// other files still reference it (then refactor them similarly).
void AuthCommand::fill_psd(net::PerSocketData& /*psd*/,
                           const infra::security::token::UserClaims& /*claims*/) {
    // no-op in new architecture
}

// subscribe_to_hubs is now done inside apply_psd
void AuthCommand::subscribe_to_hubs(const net::PerSocketData& /*psd*/,
                                    const std::vector<Hub>& /*hubs*/) const {
    // no-op in new architecture
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
        if (!other_psd->snapshot) return;

        const auto display = safe_display(*other_psd);
        if (!display.empty()) ids_.remember_display(other_psd->user_id, display);

        for (const auto& hub_id : other_psd->snapshot->hubs) {
            if (!target_hubs.empty() && !target_hubs.count(hub_id)) continue;
            ids_.to_public(hub_id);
            online_by_hub[hub_id][other_psd->user_id] = display;
        }
    });

    json result = json::object();
    for (const auto& hub : hubs) {
        const auto public_id = ids_.to_public(hub.id);
        const auto members = db_.hubs().getHubMembers(hub.id);

        json arr = json::array();
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
            for (const auto& [user_id, display0] : it_online->second) {
                if (seen.find(user_id) != seen.end()) continue;

                const auto public_user = ids_.to_public(user_id);

                std::string name = display0.empty() ? ids_.display_for(user_id) : display0;
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
