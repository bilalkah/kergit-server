#ifndef APP_SERVICES_HUB_HUBSERVICE_H
#define APP_SERVICES_HUB_HUBSERVICE_H

#include "app/services/hub/HubCache.h"
#include "app/services/hub/HubSnapshot.h"
#include "infra/persistence/repositories/ChannelRepository.h"
#include "infra/persistence/repositories/HubRepository.h"

#include <memory>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace app::services {

class HubService {
   public:
    HubService(HubRepository& repo, ChannelRepository& channel_repo);

    // ---- Reads ----
    std::optional<Hub> getHub(const HubId& hubId);
    std::vector<Hub> getUserHubs(const UserId& userId);
    std::vector<std::pair<UserId, std::string>> getHubMembers(const HubId& hubId);

    bool isHubMember(const HubId& hubId, const UserId& userId);
    std::optional<Role> getMembershipRole(const HubId& hubId, const UserId& userId);

    // ---- Writes ----
    HubId createHub(const std::string& name, const UserId& owner);
    bool renameHub(const HubId& hubId, const std::string& name);
    bool deleteHub(const HubId& hubId, const UserId& owner);

    void addMember(const HubId& hubId, const UserId& userId, Role role);
    void removeMember(const HubId& hubId, const UserId& userId);

    HubSnapshot buildSnapshot(const HubId& hubId);
    std::optional<HubSnapshot> tryGetSnapshot(const HubId& hubId) const;
    std::optional<std::pair<HubId, HubSnapshotChannel>> tryGetSnapshotChannel(
        const ChannelId& channelId) const;
    HubSnapshot getOrBuildSnapshot(const HubId& hubId);
    void invalidateSnapshot(const HubId& hubId);
    void invalidateSnapshotsForChannel(const ChannelId& channelId);

   private:
    HubRepository& repo_;
    ChannelRepository& channel_repo_;
    std::unique_ptr<IHubCache> cache_;
    mutable std::shared_mutex snapshot_mutex_;
    std::unordered_map<HubId, HubSnapshot> snapshots_;
};

}  // namespace app::services

#endif  // APP_SERVICES_HUB_HUBSERVICE_H
