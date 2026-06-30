#include "app/services/hub/HubCache.h"

namespace app::services {
HubCache::HubCache() : cache_(std::make_unique<core::cache::OnMemoryCache>()) {}

std::expected<std::shared_ptr<const HubSnapshot>, HubCacheError> HubCache::get(const HubId& hubId) {
    auto res = cache_->get(core::cache::AnyKey::make(hubId));
    if (!res.has_value()) {
        return std::unexpected(HubCacheError::NotFound);
    }

    // The any holds a shared_ptr<const HubSnapshot>; copying it out is a refcount
    // bump, not a deep copy of the member map.
    auto* snapshot = std::any_cast<std::shared_ptr<const HubSnapshot>>(&res.value());
    if (!snapshot || !*snapshot) {
        cache_->erase(core::cache::AnyKey::make(hubId));
        return std::unexpected(HubCacheError::CorruptedEntry);
    }
    return *snapshot;
}

void HubCache::put(std::shared_ptr<const HubSnapshot> snapshot) {
    if (!snapshot) return;
    const auto key = core::cache::AnyKey::make(snapshot->hub_id);
    cache_->put(key, std::any(std::move(snapshot)));
}

void HubCache::invalidate(const HubId& hubId) { cache_->erase(core::cache::AnyKey::make(hubId)); }
}  // namespace app::services
