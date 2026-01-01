#ifndef APP_SERVICES_PRESENCE_PRESENCESERVICE_H
#define APP_SERVICES_PRESENCE_PRESENCESERVICE_H

#include "app/managers/session/SessionManager.h"
#include "app/managers/subscription/SubscriptionManager.h"
#include "domains/ids/Ids.h"

#include <vector>

namespace app::services {
class PresenceService {
   public:
    PresenceService(const SessionManager& sessions, const SubscriptionManager& subs)
        : sessions_(sessions), subs_(subs) {}

    std::vector<UserId> onlineUsers() const;
    std::vector<UserId> onlineUsersInHub(const HubId&) const;
    std::vector<UserId> onlineUsersInChannel(const HubId&, const ChannelId&) const;
    bool isUserOnline(const UserId&) const;

   private:
    const SessionManager& sessions_;
    const SubscriptionManager& subs_;
};

}  // namespace app::services

#endif  // APP_SERVICES_PRESENCE_PRESENCESERVICE_H
