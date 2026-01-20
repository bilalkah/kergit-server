#ifndef APP_SERVICES_HUB_HUBSNAPSHOT_H
#define APP_SERVICES_HUB_HUBSNAPSHOT_H

#include "domains/Channel.h"
#include "domains/Hub.h"
#include "domains/ids/Ids.h"

#include <string>
#include <vector>

namespace app::services {

struct HubSnapshotChannel {
    ChannelId id;
    std::string name;
    ChannelType type{ChannelType::CHAT};
};

struct HubSnapshotMember {
    UserId user_id;
    std::string display_name;
    std::string avatar_seed;
    Role role{Role::USER};
};

struct HubSnapshot {
    HubId id;
    std::string name;
    std::string avatar_seed;
    std::vector<HubSnapshotChannel> channels;
    std::vector<HubSnapshotMember> members;
};

}  // namespace app::services

#endif  // APP_SERVICES_HUB_HUBSNAPSHOT_H
