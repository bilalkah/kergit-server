#ifndef APP_SERVICES_HUB_HUBSERVICE_H
#define APP_SERVICES_HUB_HUBSERVICE_H

#include "app/services/hub/HubCache.h"
#include "infra/persistence/repositories/HubRepository.h"

#include <memory>
#include <optional>
#include <vector>

namespace app::services {

class HubService {
   public:
    HubService(HubRepository& repo);

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

   private:
    HubRepository& repo_;
    std::unique_ptr<IHubCache> cache_;
};

}  // namespace app::services

#endif  // APP_SERVICES_HUB_HUBSERVICE_H
