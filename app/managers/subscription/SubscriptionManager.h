#ifndef APP_MANAGERS_SUBSCRIPTION_SUBSCRIPTIONMANAGER_H_
#define APP_MANAGERS_SUBSCRIPTION_SUBSCRIPTIONMANAGER_H_

#include "app/managers/subscription/ISubscriptionManager.h"

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace app {
class SubscriptionManager final : public ISubscriptionManager {
   public:
    bool subscribeConnection(const GlobalConnId& conn, const Topic& topic) override;
    bool unsubscribeConnection(const GlobalConnId& conn, const Topic& topic) override;

    bool isSubscribed(const GlobalConnId& conn, const Topic& topic) const override;

    std::shared_ptr<const std::vector<GlobalConnId>> getSubscribers(
        const Topic& topic) const override;
    std::expected<std::unordered_set<Topic>, SubscriptionError> getSubscriptionsForConnection(
        const GlobalConnId& conn) const override;

    void removeAllForConnection(const GlobalConnId& conn) override;
    void removeAllForTopic(const Topic& topic) override;

   private:
    // Subscribers are kept as a copy-on-write vector so readers get a cheap, contiguous
    // snapshot to fan out over. The conn->topics reverse index is the authoritative
    // membership set: it gives O(1) dedup on subscribe and O(1) isSubscribed without
    // scanning the subscriber vector. Subscription is connection-scoped (a user may have
    // several active connections); user-level aggregation lives elsewhere.
    using SubscriberVec = std::vector<GlobalConnId>;
    using SubscriberVecPtr = std::shared_ptr<SubscriberVec>;

    std::unordered_map<Topic, SubscriberVecPtr> topic_to_conns_;
    std::unordered_map<GlobalConnId, std::unordered_set<Topic>> conn_to_topics_;
    mutable std::shared_mutex mu_;
};

}  // namespace app

#endif  // APP_MANAGERS_SUBSCRIPTION_SUBSCRIPTIONMANAGER_H_
