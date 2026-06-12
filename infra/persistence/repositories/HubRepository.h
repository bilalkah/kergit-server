#ifndef INFRA_PERSISTENCE_REPOSITORIES_HUB_REPOSITORY_H
#define INFRA_PERSISTENCE_REPOSITORIES_HUB_REPOSITORY_H

#include "domains/Channel.h"
#include "domains/Hub.h"
#include "infra/persistence/DatabaseExecutor.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class HubRepository {
   public:
    // Lightweight member info for snapshot caching (no user details)
    struct MemberRole {
        UserId user_id;
        Role role{Role::USER};
    };

    explicit HubRepository(DatabaseExecutor& db) : db_(db) {}

    Hub createHub(const std::string& hubName, const UserId& ownerUuid);
    void addMember(const HubId& hubId, const UserId& userUuid, const std::string& role);
    void removeMember(const HubId& hubId, const UserId& userUuid);
    std::vector<Hub> getUserHubs(const UserId& userUuid);
    std::vector<Hub> getHubsByIds(const std::vector<HubId>& hubIds);
    // Metadata-only hub read (id/name/owner/avatar).
    std::optional<Hub> getHub(const HubId& hubId);
    std::optional<Channel> getChannel(const ChannelId& channelId);
    std::vector<Channel> getChannelsByIds(const std::vector<ChannelId>& channelIds);
    std::vector<Channel> getHubChannels(const HubId& hubId);
    std::unordered_map<HubId, std::vector<Channel>> getHubChannelsByHubIds(
        const std::vector<HubId>& hubIds);
    bool isHubMember(const HubId& hubId, const UserId& userUuid);
    std::optional<Role> getMembershipRole(const HubId& hubId, const UserId& userUuid);
    std::vector<MemberRole> getHubMemberRoles(const HubId& hubId);
    std::unordered_map<HubId, std::vector<MemberRole>> getHubMemberRolesByHubIds(
        const std::vector<HubId>& hubIds);
    // Preferred for hot paths: single-round-trip hub + members.
    std::optional<Hub> getHubWithMembers(const HubId& hubId);
    bool renameHub(const HubId& hubId, const std::string& name);
    bool updateHubAvatarSeed(const HubId& hubId, const std::string& avatar_seed);
    bool deleteHub(const HubId& hubId, const UserId& ownerUuid);
    HubId ensurePersonalHubWithGeneral(const UserId& ownerUuid, const std::string& hubName);
    ChannelId createChannel(const HubId& hubId, const std::string& channelName,
                            const std::string& type);
    bool renameChannel(const ChannelId& channelId, const std::string& name);
    bool deleteChannel(const ChannelId& channelId, const HubId& hubId);
    std::vector<ChannelId> getHubChannelIds(const HubId& hubId);
    std::unordered_map<HubId, std::vector<ChannelId>> getHubChannelIdsByHubIds(
        const std::vector<HubId>& hubIds);

   private:
    DatabaseExecutor& db_;
};

#endif  // INFRA_PERSISTENCE_REPOSITORIES_HUB_REPOSITORY_H
