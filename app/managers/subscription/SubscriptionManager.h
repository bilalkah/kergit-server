#ifndef APP_MANAGERS_SUBSCRIPTION_SUBSCRIPTIONMANAGER_H_
#define APP_MANAGERS_SUBSCRIPTION_SUBSCRIPTIONMANAGER_H_

#include "app/managers/subscription/ISubscriptionManager.h"

#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace app {
class SubscriptionManager final : public ISubscriptionManager {
   public:
    bool subscribe(const UserId& user, const Topic& topic) override;
    bool unsubscribe(const UserId& user, const Topic& topic) override;

    bool isSubscribed(const UserId& user, const Topic& topic) const override;

    std::expected<std::unordered_set<UserId>, SubscriptionError> getSubscribers(
        const Topic& topic) const override;
    std::expected<std::unordered_set<Topic>, SubscriptionError> getSubscriptions(
        const UserId& user) const override;

    void removeAllForUser(const UserId& user) override;
    void removeAllForTopic(const Topic& topic) override;

   private:
    std::unordered_map<Topic, std::unordered_set<UserId>> topic_to_users_;
    std::unordered_map<UserId, std::unordered_set<Topic>> user_to_topics_;
    mutable std::shared_mutex mu_;
};

}  // namespace app

#endif  // APP_MANAGERS_SUBSCRIPTION_SUBSCRIPTIONMANAGER_H_
