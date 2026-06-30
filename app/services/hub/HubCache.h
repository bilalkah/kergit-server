#ifndef APP_SERVICES_HUB_HUBCACHE_H
#define APP_SERVICES_HUB_HUBCACHE_H

#include "app/services/hub/HubSnapshot.h"
#include "core/cache/OnMemoryCache.h"
#include "domains/ids/Ids.h"

#include <expected>
#include <memory>

namespace app::services {

enum class HubCacheError { NotFound, CorruptedEntry };

class IHubCache {
   public:
    virtual ~IHubCache() = default;

    // Snapshots are served as shared_ptr<const> so the per-message membership/role
    // reads (resolveMembershipRole/isHubMember) never deep-copy the member map.
    virtual std::expected<std::shared_ptr<const HubSnapshot>, HubCacheError> get(
        const HubId& hubId) = 0;

    virtual void put(std::shared_ptr<const HubSnapshot> snapshot) = 0;
    virtual void invalidate(const HubId& hubId) = 0;
};

class HubCache final : public IHubCache {
   public:
    explicit HubCache();
    ~HubCache() override = default;

    std::expected<std::shared_ptr<const HubSnapshot>, HubCacheError> get(
        const HubId& hubId) override;

    void put(std::shared_ptr<const HubSnapshot> snapshot) override;

    void invalidate(const HubId& hubId) override;

   private:
    std::unique_ptr<core::cache::OnMemoryCache> cache_;
};

}  // namespace app::services

#endif  // APP_SERVICES_HUB_HUBCACHE_H
