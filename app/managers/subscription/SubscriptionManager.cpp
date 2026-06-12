#include "app/managers/subscription/SubscriptionManager.h"

#include "utils/Metrics.h"

namespace app {
bool SubscriptionManager::subscribeConnection(const GlobalConnId& conn, const Topic& topic) {
    std::unique_lock lock(mu_);
    auto& conns_ptr = topic_to_conns_[topic];
    if (!conns_ptr) {
        conns_ptr = std::make_shared<SubscriberSet>();
    } else if (!conns_ptr.unique()) {
        conns_ptr = std::make_shared<SubscriberSet>(*conns_ptr);
    }
    auto& conns = *conns_ptr;
    auto& topics = conn_to_topics_[conn];
    bool inserted = conns.insert(conn).second;
    topics.insert(topic);
    return inserted;
}

bool SubscriptionManager::unsubscribeConnection(const GlobalConnId& conn, const Topic& topic) {
    std::unique_lock lock(mu_);
    auto topic_it = topic_to_conns_.find(topic);
    if (topic_it == topic_to_conns_.end()) {
        return false;
    }
    auto& conns_ptr = topic_it->second;
    if (!conns_ptr.unique()) {
        conns_ptr = std::make_shared<SubscriberSet>(*conns_ptr);
    }
    auto& conns = *conns_ptr;
    size_t erased = conns.erase(conn);
    if (conns.empty()) {
        topic_to_conns_.erase(topic_it);
    }

    auto conn_it = conn_to_topics_.find(conn);
    if (conn_it != conn_to_topics_.end()) {
        auto& topics = conn_it->second;
        topics.erase(topic);
        if (topics.empty()) {
            conn_to_topics_.erase(conn_it);
        }
    }

    return erased > 0;
}

bool SubscriptionManager::isSubscribed(const GlobalConnId& conn, const Topic& topic) const {
    std::shared_lock lock(mu_);
    auto topic_it = topic_to_conns_.find(topic);
    if (topic_it == topic_to_conns_.end()) {
        return false;
    }
    const auto& conns = *topic_it->second;
    return conns.find(conn) != conns.end();
}

std::shared_ptr<const std::unordered_set<GlobalConnId>> SubscriptionManager::getSubscribers(
    const Topic& topic) const {
    std::shared_lock lock(mu_);
    auto topic_it = topic_to_conns_.find(topic);
    if (topic_it == topic_to_conns_.end()) {
        return nullptr;
    }
    return topic_it->second;
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
        if (topic_it != topic_to_conns_.end()) {
            auto& conns_ptr = topic_it->second;
            if (!conns_ptr.unique()) {
                conns_ptr = std::make_shared<SubscriberSet>(*conns_ptr);
            }
            auto& conns = *conns_ptr;
            conns.erase(conn);
            if (conns.empty()) {
                topic_to_conns_.erase(topic_it);
            }
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
            auto& topics = conn_it->second;
            topics.erase(topic);
            if (topics.empty()) {
                conn_to_topics_.erase(conn_it);
            }
        }
    }
    topic_to_conns_.erase(topic_it);
}
}  // namespace app
