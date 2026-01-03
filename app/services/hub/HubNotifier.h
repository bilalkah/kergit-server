#ifndef APP_SERVICES_HUB_HUBNOTIFIER_H
#define APP_SERVICES_HUB_HUBNOTIFIER_H

#include "app/services/PublicIdService.h"
#include "domains/ids/Ids.h"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace app::services {

class HubNotifier {
   public:
    HubNotifier(PublicIdService& ids);

    // hub-level events
    nlohmann::json hubRenamed(const HubId& hubId, const std::string& newName);
    nlohmann::json hubDeleted(const HubId& hubId);

    // membership events
    nlohmann::json memberJoined(const HubId& hubId, const UserId& userId);
    nlohmann::json memberLeft(const HubId& hubId, const UserId& userId);
    nlohmann::json memberOnline(const HubId& hubId, const UserId& userId);
    nlohmann::json memberOffline(const HubId& hubId, const UserId& userId);

    // channel events
    nlohmann::json channelCreated(const HubId& hubId, const ChannelId& channelId);
    nlohmann::json channelRenamed(const HubId& hubId, const ChannelId& channelId,
                                  const std::string& name);
    nlohmann::json channelDeleted(const HubId& hubId, const ChannelId& channelId);

   private:
    PublicIdService& ids_;
};

}  // namespace app::services

#endif  // APP_SERVICES_HUB_HUBNOTIFIER_H
