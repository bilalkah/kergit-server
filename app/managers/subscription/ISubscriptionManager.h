#ifndef APP_MANAGERS_SUBSCRIPTION_ISUBSCRIPTIONMANAGER_H_
#define APP_MANAGERS_SUBSCRIPTION_ISUBSCRIPTIONMANAGER_H_

#include "app/managers/subscription/Topic.h"
#include "domains/ids/Ids.h"

#include <expected>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace app {

using SubscriptionError = std::string;

struct ISubscriptionManager {
    virtual ~ISubscriptionManager() = default;

    virtual bool subscribeConnection(const GlobalConnId& conn, const Topic& topic) = 0;
    virtual bool unsubscribeConnection(const GlobalConnId& conn, const Topic& topic) = 0;

    virtual bool isSubscribed(const GlobalConnId& conn, const Topic& topic) const = 0;

    // Subscribers are stored and served as a contiguous vector: every caller iterates the
    // whole set (almost always to build a Target's recipient vector), so a vector is both
    // cache-friendlier to traverse and a direct copy rather than a bucket walk.
    virtual std::shared_ptr<const std::vector<GlobalConnId>> getSubscribers(
        const Topic& topic) const = 0;
    virtual std::expected<std::unordered_set<Topic>, SubscriptionError>
    getSubscriptionsForConnection(const GlobalConnId& conn) const = 0;

    virtual void removeAllForConnection(const GlobalConnId& conn) = 0;
    virtual void removeAllForTopic(const Topic& topic) = 0;
};
}  // namespace app

#endif  // APP_MANAGERS_SUBSCRIPTION_ISUBSCRIPTIONMANAGER_H_
