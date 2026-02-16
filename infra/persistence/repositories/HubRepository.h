#ifndef INFRA_PERSISTENCE_REPOSITORIES_HUB_REPOSITORY_H
#define INFRA_PERSISTENCE_REPOSITORIES_HUB_REPOSITORY_H

#include "domains/Hub.h"
#include "infra/persistence/DatabaseExecutor.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

class HubRepository {
   public:
    struct MemberSummary {
        UserId user_id;
        std::string display_name;
        std::string avatar_seed;
    };

    struct MemberWithRole {
        UserId user_id;
        std::string display_name;
        std::string avatar_seed;
        Role role{Role::USER};
    };

    // Lightweight member info for snapshot caching (no user details)
    struct MemberRole {
        UserId user_id;
        Role role{Role::USER};
    };

    explicit HubRepository(DatabaseExecutor& db) : db_(db) {}

    HubId createHub(const std::string& hubName, const UserId& ownerUuid);
    void addMember(const HubId& hubId, const UserId& userUuid, const std::string& role);
    void removeMember(const HubId& hubId, const UserId& userUuid);
    std::vector<Hub> getUserHubs(const UserId& userUuid);
    std::optional<Hub> getHub(const HubId& hubId);
    bool isHubMember(const HubId& hubId, const UserId& userUuid);
    std::optional<Role> getMembershipRole(const HubId& hubId, const UserId& userUuid);
    std::vector<MemberSummary> getHubMembers(const HubId& hubId);
    std::vector<MemberWithRole> getHubMembersWithRoles(const HubId& hubId);
    std::vector<MemberRole> getHubMemberRoles(const HubId& hubId);
    // Preferred for hot paths: single-round-trip hub + members (replaces getHub + getHubMembers).
    std::optional<Hub> getHubWithMembers(const HubId& hubId);
    // Preferred for hot paths: single query for member display + role (replaces getHubMembers +
    // getMembershipRole fanout).
    std::vector<MemberWithRole> getHubMembersFull(const HubId& hubId);
    bool renameHub(const HubId& hubId, const std::string& name);
    bool updateHubAvatarSeed(const HubId& hubId, const std::string& avatar_seed);
    bool deleteHub(const HubId& hubId, const UserId& ownerUuid);
    HubId ensurePersonalHubWithGeneral(const UserId& ownerUuid, const std::string& hubName);
    std::vector<ChannelId> getHubChannelIds(const HubId& hubId);

   private:
    DatabaseExecutor& db_;
};

#endif  // INFRA_PERSISTENCE_REPOSITORIES_HUB_REPOSITORY_H
