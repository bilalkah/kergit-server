#include "app/services/hub/HubService.h"

namespace app::services {

namespace {
std::string role_to_string(Role role) {
    switch (role) {
        case Role::OWNER:
            return "owner";
        case Role::ADMIN:
            return "admin";
        case Role::USER:
            return "member";
        default:
            return "member";
    }
}

}  // namespace

HubService::HubService(HubRepository& repo) : repo_(repo), cache_(std::make_unique<HubCache>()) {}

std::optional<Hub> HubService::getHub(const HubId& hubId) {
    auto cached = cache_->get(hubId);
    if (cached) {
        return cached.value();
    }
    auto hub = repo_.getHub(hubId);
    if (hub) {
        cache_->put(*hub);
    }
    return hub;
}

std::vector<Hub> HubService::getUserHubs(const UserId& userId) { return repo_.getUserHubs(userId); }

std::vector<std::pair<UserId, std::string>> HubService::getHubMembers(const HubId& hubId) {
    return repo_.getHubMembers(hubId);
}

bool HubService::isHubMember(const HubId& hubId, const UserId& userId) {
    return repo_.isHubMember(hubId, userId);
}
std::optional<Role> HubService::getMembershipRole(const HubId& hubId, const UserId& userId) {
    return repo_.getMembershipRole(hubId, userId);
}
HubId HubService::createHub(const std::string& name, const UserId& owner) {
    return repo_.createHub(name, owner);
}
bool HubService::renameHub(const HubId& hubId, const std::string& name) {
    auto result = repo_.renameHub(hubId, name);
    if (result) {
        cache_->invalidate(hubId);
    }
    return result;
}

bool HubService::deleteHub(const HubId& hubId, const UserId& owner) {
    auto result = repo_.deleteHub(hubId, owner);
    if (result) {
        cache_->invalidate(hubId);
    }
    return result;
}

void HubService::addMember(const HubId& hubId, const UserId& userId, Role role) {
    repo_.addMember(hubId, userId, role_to_string(role));
    cache_->invalidate(hubId);
}
void HubService::removeMember(const HubId& hubId, const UserId& userId) {
    repo_.removeMember(hubId, userId);
    cache_->invalidate(hubId);
}

}  // namespace app::services
