#include "app/services/HubPublisher.h"

#include "app/services/PublicIdService.h"
#include "utils/Logger.h"

#include <algorithm>
#include <exception>
#include <libusockets.h>
#include <unordered_map>
#include <unordered_set>

namespace app::services {

namespace {
std::string safe_display(const net::PerSocketData& psd) {
    if (!psd.username.empty()) return psd.username;
    return "Member";
}

std::string channel_type_to_string(ChannelType type) {
    return type == ChannelType::VOICE ? "voice" : "text";
}
}  // namespace

HubPublisher::HubPublisher(core::IApp& app, PersistenceGateway& db, net::ConnectionManager& connections,
                           net::ClientGateway& gateway, PublicIdService& ids,
                           std::chrono::milliseconds interval)
    : app_(app),
      db_(db),
      connections_(connections),
      gateway_(gateway),
      ids_(ids),
      interval_(interval) {}

HubPublisher::~HubPublisher() { stop(); }

void HubPublisher::start() {
    if (running_) return;
    auto* loop = app_.uws().getLoop();
    if (!loop) return;

    timer_ = us_create_timer(reinterpret_cast<us_loop_t*>(loop), 0, sizeof(HubPublisher*));
    if (!timer_) return;
    running_ = true;
    auto** slot = reinterpret_cast<HubPublisher**>(us_timer_ext(timer_));
    if (slot) *slot = this;
    us_timer_set(timer_, &HubPublisher::on_timer, static_cast<int>(interval_.count()),
                 static_cast<int>(interval_.count()));
}

void HubPublisher::stop() {
    if (!timer_) {
        running_ = false;
        return;
    }
    auto* timer = timer_;
    timer_ = nullptr;
    running_ = false;

    if (auto* loop = app_.uws().getLoop()) {
        loop->defer([timer]() {
            if (!timer) return;
            auto** slot = reinterpret_cast<HubPublisher**>(us_timer_ext(timer));
            if (slot) *slot = nullptr;
            us_timer_set(timer, nullptr, 0, 0);
            us_timer_close(timer);
        });
    } else {
        auto** slot = reinterpret_cast<HubPublisher**>(us_timer_ext(timer));
        if (slot) *slot = nullptr;
        us_timer_set(timer, nullptr, 0, 0);
        us_timer_close(timer);
    }
}

void HubPublisher::publish_hub(const HubId& hub_id) {
    if (hub_id.value.empty()) return;
    try {
        auto channels = load_channels(hub_id);
        auto online = collect_online_for_hub(hub_id);
        auto payload = build_snapshot(hub_id, channels, online);
        gateway_.publish(topic_for(hub_id), payload);
    } catch (const std::exception& ex) {
        utils::log_line(utils::LogLevel::WARN,
                        "HubPublisher failed to publish hub " + hub_id.value + ": " + ex.what());
    }
}

void HubPublisher::publish_hubs(const std::unordered_set<HubId>& hub_ids) {
    for (const auto& hub_id : hub_ids) {
        publish_hub(hub_id);
    }
}

std::string HubPublisher::topic_for(const HubId& hub_id) { return "hub:" + hub_id.value; }

void HubPublisher::on_timer(us_timer_t* timer) {
    auto** slot = reinterpret_cast<HubPublisher**>(us_timer_ext(timer));
    HubPublisher* self = slot ? *slot : nullptr;
    if (self) self->tick();
}

void HubPublisher::tick() {
    auto hubs = collect_all_hubs();
    for (const auto& hub_id : hubs) {
        publish_hub(hub_id);
    }
}

std::unordered_set<HubId> HubPublisher::collect_all_hubs() const {
    std::unordered_set<HubId> hubs;
    connections_.for_each([&](UwsSocket* ws) {
        if (!ws) return;
        auto* psd = ws->getUserData();
        if (!psd || !psd->authenticated) return;
        hubs.insert(psd->hub_memberships.begin(), psd->hub_memberships.end());
    });
    return hubs;
}

nlohmann::json HubPublisher::collect_online_for_hub(const HubId& hub_id) const {
    std::unordered_map<UserId, std::string> online_display;
    connections_.for_each([&](UwsSocket* ws) {
        if (!ws) return;
        auto* psd = ws->getUserData();
        if (!psd || !psd->authenticated) return;
        if (psd->hub_memberships.find(hub_id) == psd->hub_memberships.end()) return;
        const auto display = safe_display(*psd);
        if (!display.empty()) ids_.remember_display(psd->user_id, display);
        online_display.emplace(psd->user_id, display);
    });

    nlohmann::json arr = nlohmann::json::array();
    const auto members = db_.hubs().getHubMembers(hub_id);
    std::unordered_set<UserId> seen;
    for (const auto& [member_id, stored_display] : members) {
        const bool is_online = online_display.find(member_id) != online_display.end();
        if (!stored_display.empty()) ids_.remember_display(member_id, stored_display);
        std::string display = ids_.display_for(member_id);
        if (display.empty() && is_online) display = online_display[member_id];
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

    for (const auto& [user_id, display] : online_display) {
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

    return arr;
}

std::vector<Channel> HubPublisher::load_channels(const HubId& hub_id) const {
    return db_.channels().getHubChannels(hub_id);
}

nlohmann::json HubPublisher::build_snapshot(const HubId& hub_id,
                                            const std::vector<Channel>& channels,
                                            const nlohmann::json& online) const {
    const auto public_hub_id = ids_.to_public(hub_id);
    nlohmann::json chan = nlohmann::json::array();
    for (const auto& c : channels) {
        const auto public_channel_id = ids_.to_public(c.channel_id);
        const auto public_channel_hub_id = ids_.to_public(c.hub_id);
        chan.push_back({{"id", public_channel_id.value},
                        {"hub_id", public_channel_hub_id.value},
                        {"name", c.name},
                        {"type", channel_type_to_string(c.type)}});
    }
    return {
        {"type", "hub_snapshot"},
        {"hub_id", public_hub_id.value},
        {"channels", std::move(chan)},
        {"online", online},
    };
}

}  // namespace app::services
