#ifndef APP_SERVICES_HUB_HUBSNAPSHOT_H
#define APP_SERVICES_HUB_HUBSNAPSHOT_H

#include "domains/Hub.h"
#include "domains/ids/Ids.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace app::services {

struct HubSnapshot {
    HubId hub_id;
    UserId owner_id;
    std::string name;
    std::string avatar_seed;
    // Channel identity only; channel fields are resolved via HubService channel cache/reads.
    std::vector<ChannelId> channel_ids;
    std::unordered_map<UserId, Role, UserIdHash, UserIdEq> member_roles;
};

}  // namespace app::services

#endif  // APP_SERVICES_HUB_HUBSNAPSHOT_H
