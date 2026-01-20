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
    struct MemberWithRole {
        UserId user_id;
        std::string display_name;
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
    std::vector<std::pair<UserId, std::string>> getHubMembers(const HubId& hubId);
    std::vector<MemberWithRole> getHubMembersWithRoles(const HubId& hubId);
    bool renameHub(const HubId& hubId, const std::string& name);
    bool deleteHub(const HubId& hubId, const UserId& ownerUuid);
    HubId ensurePersonalHubWithGeneral(const UserId& ownerUuid, const std::string& hubName);
    std::vector<ChannelId> getHubChannelIds(const HubId& hubId);

   private:
    DatabaseExecutor& db_;
};

#endif  // INFRA_PERSISTENCE_REPOSITORIES_HUB_REPOSITORY_H
