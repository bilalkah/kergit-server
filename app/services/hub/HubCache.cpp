#include "app/services/hub/HubCache.h"

namespace app::services {
HubCache::HubCache() : cache_(std::make_unique<core::cache::OnMemoryCache>()) {}

std::expected<Hub, HubCacheError> HubCache::get(const HubId& hubId) {
    auto res = cache_->get(core::cache::AnyKey::make(hubId));
    if (!res.has_value()) {
        return std::unexpected(HubCacheError::NotFound);
    }

    auto* hub = std::any_cast<Hub>(&res.value());
    if (!hub) {
        cache_->erase(core::cache::AnyKey::make(hubId));
        return std::unexpected(HubCacheError::CorruptedEntry);
    }
    return *hub;
}

void HubCache::put(const Hub& hub) { cache_->put(core::cache::AnyKey::make(hub.id), hub); }

void HubCache::invalidate(const HubId& hubId) { cache_->erase(core::cache::AnyKey::make(hubId)); }
}  // namespace app::services
