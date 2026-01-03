#ifndef APP_SERVICES_HUB_HUBCACHE_H
#define APP_SERVICES_HUB_HUBCACHE_H

#include "core/cache/OnMemoryCache.h"
#include "domains/Hub.h"
#include "domains/ids/Ids.h"

#include <expected>
#include <memory>

namespace app::services {

enum class HubCacheError { NotFound, CorruptedEntry };

class IHubCache {
   public:
    virtual ~IHubCache() = default;

    virtual std::expected<Hub, HubCacheError> get(const HubId& hubId) = 0;

    virtual void put(const Hub& hub) = 0;
    virtual void invalidate(const HubId& hubId) = 0;
};

class HubCache final : public IHubCache {
   public:
    explicit HubCache();
    ~HubCache() override = default;

    std::expected<Hub, HubCacheError> get(const HubId& hubId) override;

    void put(const Hub& hub) override;

    void invalidate(const HubId& hubId) override;

   private:
    std::unique_ptr<core::cache::OnMemoryCache> cache_;
};

}  // namespace app::services

#endif  // APP_SERVICES_HUB_HUBCACHE_H
