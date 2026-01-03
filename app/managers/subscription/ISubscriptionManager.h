#ifndef APP_MANAGERS_SUBSCRIPTION_ISUBSCRIPTIONMANAGER_H_
#define APP_MANAGERS_SUBSCRIPTION_ISUBSCRIPTIONMANAGER_H_

#include "app/managers/subscription/Topic.h"
#include "domains/ids/Ids.h"

#include <expected>
#include <string>
#include <unordered_set>

namespace app {

using SubscriptionError = std::string;

struct ISubscriptionManager {
    virtual ~ISubscriptionManager() = default;

    virtual bool subscribe(const UserId& user, const Topic& topic) = 0;
    virtual bool unsubscribe(const UserId& user, const Topic& topic) = 0;

    virtual bool isSubscribed(const UserId& user, const Topic& topic) const = 0;

    virtual std::expected<std::unordered_set<UserId>, SubscriptionError> getSubscribers(
        const Topic& topic) const = 0;
    virtual std::expected<std::unordered_set<Topic>, SubscriptionError> getSubscriptions(
        const UserId& user) const = 0;

    virtual void removeAllForUser(const UserId& user) = 0;
    virtual void removeAllForTopic(const Topic& topic) = 0;
};
}  // namespace app

#endif  // APP_MANAGERS_SUBSCRIPTION_ISUBSCRIPTIONMANAGER_H_
