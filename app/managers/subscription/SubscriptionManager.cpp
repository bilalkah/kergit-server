#include "app/managers/subscription/SubscriptionManager.h"

#include <algorithm>

namespace app {
namespace {

// Remove `conn` from a COW subscriber vector, copying first if a reader still holds the
// current snapshot. Order is irrelevant to fan-out, so erase is O(1) swap-with-last.
// Returns true if the vector became empty (caller drops the topic entry).
bool eraseConnFromTopic(std::shared_ptr<std::vector<GlobalConnId>>& vec_ptr,
                        const GlobalConnId& conn) {
    if (vec_ptr.use_count() > 1) {
        vec_ptr = std::make_shared<std::vector<GlobalConnId>>(*vec_ptr);
    }
    auto& vec = *vec_ptr;
    auto it = std::find(vec.begin(), vec.end(), conn);
    if (it != vec.end()) {
        *it = std::move(vec.back());
        vec.pop_back();
    }
    return vec.empty();
}

}  // namespace

bool SubscriptionManager::subscribeConnection(const GlobalConnId& conn, const Topic& topic) {
    std::unique_lock lock(mu_);
    // Dedup via the reverse index (O(1)); only touch the subscriber vector on a real insert.
    auto& topics = conn_to_topics_[conn];
    if (!topics.insert(topic).second) {
        return false;  // already subscribed
    }

    auto& vec_ptr = topic_to_conns_[topic];
    if (!vec_ptr) {
        vec_ptr = std::make_shared<SubscriberVec>();
    } else if (vec_ptr.use_count() > 1) {
        // A reader is holding this snapshot — copy before mutating (COW).
        vec_ptr = std::make_shared<SubscriberVec>(*vec_ptr);
    }
    vec_ptr->push_back(conn);
    return true;
}

bool SubscriptionManager::unsubscribeConnection(const GlobalConnId& conn, const Topic& topic) {
    std::unique_lock lock(mu_);
    auto conn_it = conn_to_topics_.find(conn);
    if (conn_it == conn_to_topics_.end() || conn_it->second.erase(topic) == 0) {
        return false;  // was not subscribed
    }
    if (conn_it->second.empty()) {
        conn_to_topics_.erase(conn_it);
    }

    auto topic_it = topic_to_conns_.find(topic);
    if (topic_it != topic_to_conns_.end() && eraseConnFromTopic(topic_it->second, conn)) {
        topic_to_conns_.erase(topic_it);
    }
    return true;
}

bool SubscriptionManager::isSubscribed(const GlobalConnId& conn, const Topic& topic) const {
    std::shared_lock lock(mu_);
    auto conn_it = conn_to_topics_.find(conn);
    return conn_it != conn_to_topics_.end() && conn_it->second.contains(topic);
}

std::shared_ptr<const std::vector<GlobalConnId>> SubscriptionManager::getSubscribers(
    const Topic& topic) const {
    std::shared_lock lock(mu_);
    auto topic_it = topic_to_conns_.find(topic);
    if (topic_it == topic_to_conns_.end()) {
        return nullptr;
    }
    return topic_it->second;  // shared_ptr<SubscriberVec> -> shared_ptr<const SubscriberVec>
}

std::expected<std::unordered_set<Topic>, SubscriptionError>
SubscriptionManager::getSubscriptionsForConnection(const GlobalConnId& conn) const {
    std::shared_lock lock(mu_);
    auto conn_it = conn_to_topics_.find(conn);
    if (conn_it == conn_to_topics_.end()) {
        return std::unexpected("Connection not found");
    }
    return conn_it->second;
}

void SubscriptionManager::removeAllForConnection(const GlobalConnId& conn) {
    std::unique_lock lock(mu_);
    auto conn_it = conn_to_topics_.find(conn);
    if (conn_it == conn_to_topics_.end()) {
        return;
    }
    for (const auto& topic : conn_it->second) {
        auto topic_it = topic_to_conns_.find(topic);
        if (topic_it != topic_to_conns_.end() && eraseConnFromTopic(topic_it->second, conn)) {
            topic_to_conns_.erase(topic_it);
        }
    }
    conn_to_topics_.erase(conn_it);
}

void SubscriptionManager::removeAllForTopic(const Topic& topic) {
    std::unique_lock lock(mu_);
    auto topic_it = topic_to_conns_.find(topic);
    if (topic_it == topic_to_conns_.end()) {
        return;
    }
    for (const auto& conn : *topic_it->second) {
        auto conn_it = conn_to_topics_.find(conn);
        if (conn_it != conn_to_topics_.end()) {
            conn_it->second.erase(topic);
            if (conn_it->second.empty()) {
                conn_to_topics_.erase(conn_it);
            }
        }
    }
    topic_to_conns_.erase(topic_it);
}
}  // namespace app
