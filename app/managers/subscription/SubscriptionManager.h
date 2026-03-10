#ifndef APP_MANAGERS_SUBSCRIPTION_SUBSCRIPTIONMANAGER_H_
#define APP_MANAGERS_SUBSCRIPTION_SUBSCRIPTIONMANAGER_H_

#include "app/managers/subscription/ISubscriptionManager.h"

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace app {
class SubscriptionManager final : public ISubscriptionManager {
   public:
    bool subscribeConnection(const GlobalConnId& conn, const Topic& topic) override;
    bool unsubscribeConnection(const GlobalConnId& conn, const Topic& topic) override;

    bool isSubscribed(const GlobalConnId& conn, const Topic& topic) const override;

    std::shared_ptr<const std::unordered_set<GlobalConnId>> getSubscribers(
        const Topic& topic) const override;
    std::expected<std::unordered_set<Topic>, SubscriptionError> getSubscriptionsForConnection(
        const GlobalConnId& conn) const override;

    void removeAllForConnection(const GlobalConnId& conn) override;
    void removeAllForTopic(const Topic& topic) override;

   private:
    using SubscriberSet = std::unordered_set<GlobalConnId>;
    using SubscriberSetPtr = std::shared_ptr<SubscriberSet>;

    // Subscriptions are connection-scoped.
    // Each subscription represents a single active connection.
    // A single user may have multiple simultaneous active connections.
    // User-level aggregation (presence, UI) is handled elsewhere.
    std::unordered_map<Topic, SubscriberSetPtr> topic_to_conns_;
    std::unordered_map<GlobalConnId, std::unordered_set<Topic>> conn_to_topics_;
    mutable std::shared_mutex mu_;
};

}  // namespace app

#endif  // APP_MANAGERS_SUBSCRIPTION_SUBSCRIPTIONMANAGER_H_
