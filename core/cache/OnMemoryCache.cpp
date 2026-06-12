#include "core/cache/OnMemoryCache.h"

namespace core::cache {
bool OnMemoryCache::contains(const AnyKey& key) const {
    std::shared_lock lock(mu_);
    return map_.contains(key);
}

std::expected<std::any, CacheError> OnMemoryCache::get(const AnyKey& key) {
    std::shared_lock lock(mu_);
    auto it = map_.find(key);
    if (it == map_.end()) return std::unexpected<CacheError>("Key not found");
    return it->second;
}

void OnMemoryCache::put(const AnyKey& key, std::any value) {
    std::unique_lock lock(mu_);
    map_[key] = std::move(value);
}

void OnMemoryCache::erase(const AnyKey& key) {
    std::unique_lock lock(mu_);
    map_.erase(key);
}

void OnMemoryCache::clear() {
    std::unique_lock lock(mu_);
    map_.clear();
}

bool OnMemoryCache::update(const AnyKey& key, const std::function<void(std::any&)>& fn) {
    std::unique_lock lock(mu_);
    auto it = map_.find(key);
    if (it == map_.end()) return false;
    fn(it->second);
    return true;
}

std::size_t OnMemoryCache::size() const {
    std::shared_lock lock(mu_);
    return map_.size();
}
}  // namespace core::cache
