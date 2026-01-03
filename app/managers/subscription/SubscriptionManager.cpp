#include "app/managers/subscription/SubscriptionManager.h"

namespace app {
bool SubscriptionManager::subscribe(const UserId& user, const Topic& topic) {
    std::unique_lock lock(mu_);
    auto& users = topic_to_users_[topic];
    auto& topics = user_to_topics_[user];
    bool inserted = users.insert(user).second;
    topics.insert(topic);
    return inserted;
}

bool SubscriptionManager::unsubscribe(const UserId& user, const Topic& topic) {
    std::unique_lock lock(mu_);
    auto topic_it = topic_to_users_.find(topic);
    if (topic_it == topic_to_users_.end()) {
        return false;
    }
    auto& users = topic_it->second;
    size_t erased = users.erase(user);
    if (users.empty()) {
        topic_to_users_.erase(topic_it);
    }

    auto user_it = user_to_topics_.find(user);
    if (user_it != user_to_topics_.end()) {
        auto& topics = user_it->second;
        topics.erase(topic);
        if (topics.empty()) {
            user_to_topics_.erase(user_it);
        }
    }

    return erased > 0;
}

bool SubscriptionManager::isSubscribed(const UserId& user, const Topic& topic) const {
    std::shared_lock lock(mu_);
    auto topic_it = topic_to_users_.find(topic);
    if (topic_it == topic_to_users_.end()) {
        return false;
    }
    const auto& users = topic_it->second;
    return users.find(user) != users.end();
}

std::expected<std::unordered_set<UserId>, SubscriptionError> SubscriptionManager::getSubscribers(
    const Topic& topic) const {
    std::shared_lock lock(mu_);
    auto topic_it = topic_to_users_.find(topic);
    if (topic_it == topic_to_users_.end()) {
        return std::unexpected("Topic not found");
    }
    return topic_it->second;
}

std::expected<std::unordered_set<Topic>, SubscriptionError> SubscriptionManager::getSubscriptions(
    const UserId& user) const {
    std::shared_lock lock(mu_);
    auto user_it = user_to_topics_.find(user);
    if (user_it == user_to_topics_.end()) {
        return std::unexpected("User not found");
    }
    return user_it->second;
}

void SubscriptionManager::removeAllForUser(const UserId& user) {
    std::unique_lock lock(mu_);
    auto user_it = user_to_topics_.find(user);
    if (user_it == user_to_topics_.end()) {
        return;
    }
    for (const auto& topic : user_it->second) {
        auto topic_it = topic_to_users_.find(topic);
        if (topic_it != topic_to_users_.end()) {
            auto& users = topic_it->second;
            users.erase(user);
            if (users.empty()) {
                topic_to_users_.erase(topic_it);
            }
        }
    }
    user_to_topics_.erase(user_it);
}

void SubscriptionManager::removeAllForTopic(const Topic& topic) {
    std::unique_lock lock(mu_);
    auto topic_it = topic_to_users_.find(topic);
    if (topic_it == topic_to_users_.end()) {
        return;
    }
    for (const auto& user : topic_it->second) {
        auto user_it = user_to_topics_.find(user);
        if (user_it != user_to_topics_.end()) {
            auto& topics = user_it->second;
            topics.erase(topic);
            if (topics.empty()) {
                user_to_topics_.erase(user_it);
            }
        }
    }
    topic_to_users_.erase(topic_it);
}
}  // namespace app
