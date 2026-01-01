#ifndef APP_SERVICES_HUB_SNAPSHOT_BUILDER_H
#define APP_SERVICES_HUB_SNAPSHOT_BUILDER_H

#include "app/services/PublicIdService.h"
#include "app/services/channel/ChannelService.h"
#include "app/services/hub/HubService.h"
#include "app/services/presence/PresenceService.h"
#include "domains/Channel.h"
#include "domains/ids/Ids.h"

#include <nlohmann/json.hpp>

namespace app::services {

class HubSnapshotBuilder {
   public:
    HubSnapshotBuilder(ChannelService& channel_servise, HubService& hub_service,
                       PresenceService& presence, PublicIdService& ids);

    nlohmann::json build(const HubId& hub_id);

   private:
    nlohmann::json build_channels(const HubId& hub_id);
    nlohmann::json build_members(const HubId& hub_id);

    ChannelService& channel_servise_;
    HubService& hub_service_;
    PresenceService& presence_;
    PublicIdService& ids_;
};

}  // namespace app::services

#endif  // APP_SERVICES_HUB_SNAPSHOT_BUILDER_H
