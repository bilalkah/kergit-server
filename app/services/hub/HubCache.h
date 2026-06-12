#ifndef APP_SERVICES_HUB_HUBCACHE_H
#define APP_SERVICES_HUB_HUBCACHE_H

#include "core/cache/OnMemoryCache.h"
#include "app/services/hub/HubSnapshot.h"
#include "domains/ids/Ids.h"

#include <expected>
#include <memory>

namespace app::services {

enum class HubCacheError { NotFound, CorruptedEntry };

class IHubCache {
   public:
    virtual ~IHubCache() = default;

    virtual std::expected<HubSnapshot, HubCacheError> get(const HubId& hubId) = 0;

    virtual void put(const HubSnapshot& snapshot) = 0;
    virtual void invalidate(const HubId& hubId) = 0;
};

class HubCache final : public IHubCache {
   public:
    explicit HubCache();
    ~HubCache() override = default;

    std::expected<HubSnapshot, HubCacheError> get(const HubId& hubId) override;

    void put(const HubSnapshot& snapshot) override;

    void invalidate(const HubId& hubId) override;

   private:
    std::unique_ptr<core::cache::OnMemoryCache> cache_;
};

}  // namespace app::services

#endif  // APP_SERVICES_HUB_HUBCACHE_H
