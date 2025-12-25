#include "app/services/HubPublisher.h"

#include "app/services/PublicIdService.h"
#include "net/PerSocketData.h"
#include "utils/Logger.h"

#include <algorithm>
#include <exception>
#include <unordered_map>
#include <unordered_set>

namespace app::services {

namespace {
std::string safe_display(const net::PerSocketData& psd) {
    if (!psd.snapshot->username.empty()) return psd.snapshot->username;
    return "Member";
}

std::string channel_type_to_string(ChannelType type) {
    return type == ChannelType::VOICE ? "voice" : "text";
}
}  // namespace

HubPublisher::HubPublisher(core::IApp& app, PersistenceGateway& db,
                           net::ConnectionManager& connections, OutgoingQueue& out_queue,
                           PublicIdService& ids, std::chrono::milliseconds interval)
    : db_(db), connections_(connections), out_queue_(out_queue), ids_(ids), interval_(interval) {
    (void)app;  // app was previously used for uWS loop; now unused
}

HubPublisher::~HubPublisher() { stop(); }

void HubPublisher::start() {
    std::lock_guard<std::mutex> lock(mu_);
    if (running_) return;
    running_ = true;
    stop_flag_.store(false);
    worker_ = std::thread([this] { this->run(); });
}

void HubPublisher::stop() {
    {
        std::lock_guard<std::mutex> lock(mu_);
        if (!running_) return;
        running_ = false;
        stop_flag_.store(true);
    }
    cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
}

void HubPublisher::publish_hub(const HubId& hub_id) {
    if (hub_id.value.empty()) return;
    {
        std::lock_guard<std::mutex> lock(mu_);
        dirty_hubs_.insert(hub_id);
    }
    cv_.notify_all();
}

void HubPublisher::publish_hubs(const std::unordered_set<HubId>& hub_ids) {
    {
        std::lock_guard<std::mutex> lock(mu_);
        for (const auto& hub_id : hub_ids) {
            if (!hub_id.value.empty()) {
                dirty_hubs_.insert(hub_id);
            }
        }
    }
    cv_.notify_all();
}

std::string HubPublisher::topic_for(const HubId& hub_id) { return "hub:" + hub_id.value; }

// Background worker
void HubPublisher::run() {
    std::unique_lock<std::mutex> lock(mu_);
    while (!stop_flag_.load()) {
        // Wait either for interval or for new dirty hubs / stop
        cv_.wait_for(lock, interval_, [this] { return stop_flag_.load() || !dirty_hubs_.empty(); });

        if (stop_flag_.load()) break;

        std::unordered_set<HubId> hubs_to_publish;

        if (!dirty_hubs_.empty()) {
            // Take the dirty hubs and clear the set
            hubs_to_publish = std::move(dirty_hubs_);
            dirty_hubs_.clear();
        } else {
            // No explicit dirty hubs – do a periodic full refresh like before
            lock.unlock();
            hubs_to_publish = collect_all_hubs();
            lock.lock();
        }

        lock.unlock();

        for (const auto& hub_id : hubs_to_publish) {
            if (hub_id.value.empty()) continue;
            try {
                auto channels = load_channels(hub_id);
                auto online = collect_online_for_hub(hub_id);
                auto payload = build_snapshot(hub_id, channels, online);

                // Push as PublishMessage into OutgoingQueue
                out_queue_.push(OutgoingMessage{PublishMessage{topic_for(hub_id), payload.dump()}});

            } catch (const std::exception& ex) {
                utils::log_line(utils::LogLevel::WARN, "HubPublisher failed to publish hub " +
                                                           hub_id.value + ": " + ex.what());
            }
        }

        lock.lock();
    }
}

std::unordered_set<HubId> HubPublisher::collect_all_hubs() const {
    std::unordered_set<HubId> hubs;
    connections_.for_each([&](UwsSocket* ws) {
        if (!ws) return;
        auto* psd = ws->getUserData();
        if (!psd || !psd->snapshot->authenticated) return;
        if (!psd->snapshot) return;
        hubs.insert(psd->snapshot->hubs.begin(), psd->snapshot->hubs.end());
    });
    return hubs;
}

nlohmann::json HubPublisher::collect_online_for_hub(const HubId& hub_id) const {
    std::unordered_map<UserId, std::string> online_display;

    connections_.for_each([&](UwsSocket* ws) {
        if (!ws) return;
        auto* psd = ws->getUserData();
        if (!psd || !psd->snapshot->authenticated) return;
        if (!psd->snapshot) return;
        if (psd->snapshot->hubs.find(hub_id) == psd->snapshot->hubs.end()) return;

        const auto display = safe_display(*psd);
        if (!display.empty()) ids_.remember_display(psd->snapshot->user_id, display);
        online_display.emplace(psd->snapshot->user_id, display);
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
        const auto public_channel_id = ids_.to_public(c.id);
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
