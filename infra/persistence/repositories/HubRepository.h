#ifndef INFRA_PERSISTENCE_REPOSITORIES_HUB_REPOSITORY_H
#define INFRA_PERSISTENCE_REPOSITORIES_HUB_REPOSITORY_H

#include "domains/Hub.h"
#include "infra/persistence/RepositoryMux.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

class HubRepository {
   public:
    explicit HubRepository(RepositoryMux& mux) : mux_(mux) {}

    HubId createHub(const std::string& hubName, const UserId& ownerUuid);
    void addMember(const HubId& hubId, const UserId& userUuid, const std::string& role);
    void removeMember(const HubId& hubId, const UserId& userUuid);
    std::vector<Hub> getUserHubs(const UserId& userUuid);
    std::optional<Hub> getHub(const HubId& hubId);
    bool isHubMember(const HubId& hubId, const UserId& userUuid);
    std::optional<Role> getMembershipRole(const HubId& hubId, const UserId& userUuid);
    std::vector<std::pair<UserId, std::string>> getHubMembers(const HubId& hubId);
    bool renameHub(const HubId& hubId, const std::string& name);
    bool deleteHub(const HubId& hubId, const UserId& ownerUuid);
    HubId ensurePersonalHubWithGeneral(const UserId& ownerUuid, const std::string& hubName);

   private:
    RepositoryMux& mux_;
};

#endif  // INFRA_PERSISTENCE_REPOSITORIES_HUB_REPOSITORY_H
