#ifndef APP_SERVICES_HUB_HUBNOTIFIER_H
#define APP_SERVICES_HUB_HUBNOTIFIER_H

#include "domains/Channel.h"
#include "domains/Hub.h"
#include "domains/ids/Ids.h"

#include <string>
#include <vector>

namespace app::services {

class HubNotifier {
   public:
    HubNotifier() = default;

    // hub-level events (HubUpdated with action enum)
    std::string hubUpdated(const HubId& hubId, const std::string& name,
                           const std::string& avatar_seed);
    std::string hubRemoved(const HubId& hubId);

    // membership events
    std::string memberJoined(const HubId& hubId, const UserId& userId, Role role,
                             const std::string& username, const std::string& avatar_seed,
                             bool is_online);
    std::string memberLeft(const HubId& hubId, const UserId& userId);
    std::string memberOnline(const HubId& hubId, const UserId& userId);
    std::string memberOffline(const HubId& hubId, const UserId& userId);

    // channel events (ChannelUpdated with action enum)
    std::string channelCreated(const HubId& hubId, const Channel& channel);
    std::string channelUpdated(const HubId& hubId, const Channel& channel);
    std::string channelRemoved(const HubId& hubId, const ChannelId& channelId);
};

}  // namespace app::services

#endif  // APP_SERVICES_HUB_HUBNOTIFIER_H
