#ifndef APP_SERVICES_HUB_HUBNOTIFIER_H
#define APP_SERVICES_HUB_HUBNOTIFIER_H

#include "app/services/PublicIdService.h"
#include "domains/Channel.h"
#include "domains/ids/Ids.h"
#include "domains/Hub.h"

#include <string>
#include <vector>

namespace app::services {

class HubNotifier {
   public:
    HubNotifier(PublicIdService& ids);

    // hub-level events
    std::string hubRenamed(const HubId& hubId, const std::string& newName);
    std::string hubDeleted(const HubId& hubId);

    // membership events
    std::string memberJoined(const HubId& hubId,
                             const UserId& userId,
                             Role role,
                             const std::string& display_name,
                             const std::string& avatar_seed,
                             const std::string& username,
                             bool is_online);
    std::string memberLeft(const HubId& hubId, const UserId& userId);
    std::string memberOnline(const HubId& hubId, const UserId& userId);
    std::string memberOffline(const HubId& hubId, const UserId& userId);

    // channel events
    std::string channelCreated(const HubId& hubId, const Channel& channel);
    std::string channelRenamed(const HubId& hubId, const Channel& channel);
    std::string channelDeleted(const HubId& hubId, const ChannelId& channelId);

   private:
    PublicIdService& ids_;
};

}  // namespace app::services

#endif  // APP_SERVICES_HUB_HUBNOTIFIER_H
