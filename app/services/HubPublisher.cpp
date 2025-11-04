#include "app/services/HubPublisher.h"

#include "utils/Logger.h"

#include <libusockets.h>

#include <algorithm>
#include <exception>

namespace app::services {

namespace {
std::string safe_display(const net::PerSocketData& psd) {
    if (!psd.username.empty()) return psd.username;
    return "Member";
}

nlohmann::json make_member_payload(const net::PerSocketData& psd) {
    const auto name = safe_display(psd);
    return nlohmann::json{{"handle", name}, {"display_name", name}, {"online", true}};
}

std::string channel_type_to_string(ChannelType type) {
    return type == ChannelType::VOICE ? "voice" : "text";
}
}  // namespace

HubPublisher::HubPublisher(core::IApp& app, ChatDB& db, net::ConnectionManager& connections,
                           net::ClientGateway& gateway, std::chrono::milliseconds interval)
    : app_(app),
      db_(db),
      connections_(connections),
      gateway_(gateway),
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

std::string HubPublisher::topic_for(const HubId& hub_id) {
    return "hub:" + hub_id.value;
}

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
    nlohmann::json arr = nlohmann::json::array();
    connections_.for_each([&](UwsSocket* ws) {
        if (!ws) return;
        auto* psd = ws->getUserData();
        if (!psd || !psd->authenticated) return;
        if (psd->hub_memberships.find(hub_id) == psd->hub_memberships.end()) return;
        arr.push_back(make_member_payload(*psd));
    });
    return arr;
}

std::vector<Channel> HubPublisher::load_channels(const HubId& hub_id) const {
    return db_.getHubChannels(hub_id);
}

nlohmann::json HubPublisher::build_snapshot(const HubId& hub_id,
                                            const std::vector<Channel>& channels,
                                            const nlohmann::json& online) const {
    nlohmann::json chan = nlohmann::json::array();
    for (const auto& c : channels) {
        chan.push_back({{"id", c.channel_id.value},
                        {"hub_id", c.hub_id.value},
                        {"name", c.name},
                        {"type", channel_type_to_string(c.type)}});
    }
    return {
        {"type", "hub_snapshot"},
        {"hub_id", hub_id.value},
        {"channels", std::move(chan)},
        {"online", online},
    };
}

}  // namespace app::services
