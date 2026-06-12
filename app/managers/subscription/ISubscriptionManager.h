#ifndef APP_MANAGERS_SUBSCRIPTION_ISUBSCRIPTIONMANAGER_H_
#define APP_MANAGERS_SUBSCRIPTION_ISUBSCRIPTIONMANAGER_H_

#include "app/managers/subscription/Topic.h"
#include "domains/ids/Ids.h"

#include <expected>
#include <memory>
#include <string>
#include <unordered_set>

namespace app {

using SubscriptionError = std::string;

struct ISubscriptionManager {
    virtual ~ISubscriptionManager() = default;

    virtual bool subscribeConnection(const GlobalConnId& conn, const Topic& topic) = 0;
    virtual bool unsubscribeConnection(const GlobalConnId& conn, const Topic& topic) = 0;

    virtual bool isSubscribed(const GlobalConnId& conn, const Topic& topic) const = 0;

    virtual std::shared_ptr<const std::unordered_set<GlobalConnId>> getSubscribers(
        const Topic& topic) const = 0;
    virtual std::expected<std::unordered_set<Topic>, SubscriptionError>
    getSubscriptionsForConnection(const GlobalConnId& conn) const = 0;

    virtual void removeAllForConnection(const GlobalConnId& conn) = 0;
    virtual void removeAllForTopic(const Topic& topic) = 0;
};
}  // namespace app

#endif  // APP_MANAGERS_SUBSCRIPTION_ISUBSCRIPTIONMANAGER_H_
