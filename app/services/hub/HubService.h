#ifndef APP_SERVICES_HUB_HUBSERVICE_H
#define APP_SERVICES_HUB_HUBSERVICE_H

#include "app/services/hub/HubCache.h"
#include "app/services/hub/HubSnapshot.h"
#include "infra/persistence/repositories/HubRepository.h"

#include <memory>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace app::services {

struct HubMemberWithRole {
    UserId user_id;
    Role role{Role::USER};
};

struct HubTopology {
    std::vector<HubMemberWithRole> members;
    std::vector<ChannelId> channel_ids;
};

class HubService {
   public:
    explicit HubService(HubRepository& repo);

    // ---- Reads ----
    std::optional<Hub> getHub(const HubId& hubId);
    std::unordered_map<HubId, Hub> getHubsByIds(const std::vector<HubId>& hubIds);
    std::optional<Channel> getChannel(const ChannelId& channelId);
    std::unordered_map<ChannelId, Channel> getChannelsByIds(
        const std::vector<ChannelId>& channelIds);
    std::vector<Channel> getHubChannels(const HubId& hubId);
    std::vector<Hub> getUserHubs(const UserId& userId);
    void warmSnapshotsForHubs(const std::vector<Hub>& hubs);
    HubTopology getHubTopology(const HubId& hubId);
    std::vector<HubMemberWithRole> getHubMembersWithRoles(const HubId& hubId);
    std::vector<ChannelId> getHubChannelIds(const HubId& hubId);

    bool isHubMember(const HubId& hubId, const UserId& userId);
    std::optional<Role> resolveMembershipRole(const HubId& hubId, const UserId& userId);
    std::optional<Role> getMembershipRole(const HubId& hubId, const UserId& userId);

    // ---- Writes ----
    Hub createHub(const std::string& name, const UserId& owner);
    ChannelId createChannel(const HubId& hubId, const std::string& name, const std::string& type,
                            const UserId& created_by);
    bool renameChannel(const ChannelId& channelId, const std::string& newName);
    bool deleteChannel(const ChannelId& channelId, const HubId& hubId);
    bool renameHub(const HubId& hubId, const std::string& name);
    bool updateHubAvatarSeed(const HubId& hubId, const std::string& avatar_seed);
    bool deleteHub(const HubId& hubId, const UserId& owner);

    void addMember(const HubId& hubId, const UserId& userId, Role role);
    void removeMember(const HubId& hubId, const UserId& userId);

    void invalidateSnapshot(const HubId& hubId);

   private:
    HubRepository& repo_;
    std::unique_ptr<IHubCache> cache_;

    std::optional<HubSnapshot> loadSnapshot(const HubId& hubId);
    std::optional<HubSnapshot> buildSnapshot(const HubId& hubId);
    std::optional<Channel> readChannelCache(const ChannelId& channelId) const;
    void upsertChannelCache(const Channel& channel);
    void invalidateChannelCache(const ChannelId& channelId);
    void invalidateHubCaches(const HubId& hubId);

    mutable std::shared_mutex channel_cache_mutex_;
    std::unordered_map<ChannelId, Channel> channel_cache_;
};

}  // namespace app::services

#endif  // APP_SERVICES_HUB_HUBSERVICE_H
