#include "app/services/hub/HubCache.h"

namespace app::services {
HubCache::HubCache() : cache_(std::make_unique<core::cache::OnMemoryCache>()) {}

std::expected<HubSnapshot, HubCacheError> HubCache::get(const HubId& hubId) {
    auto res = cache_->get(core::cache::AnyKey::make(hubId));
    if (!res.has_value()) {
        return std::unexpected(HubCacheError::NotFound);
    }

    auto* snapshot = std::any_cast<HubSnapshot>(&res.value());
    if (!snapshot) {
        cache_->erase(core::cache::AnyKey::make(hubId));
        return std::unexpected(HubCacheError::CorruptedEntry);
    }
    return *snapshot;
}

void HubCache::put(const HubSnapshot& snapshot) {
    cache_->put(core::cache::AnyKey::make(snapshot.hub_id), snapshot);
}

void HubCache::invalidate(const HubId& hubId) { cache_->erase(core::cache::AnyKey::make(hubId)); }
}  // namespace app::services
