#include "app/memory/OnMemoryCache.h"

#include <mutex>

namespace app::memory {

OnMemoryCache::OnMemoryCache() : cache_ptr_(std::make_unique<std::unordered_map<Key, Value>>()) {
    std::unique_lock lock(mutex_);
    cache_ptr_->max_load_factor(0.7f);
    cache_ptr_->reserve(expected_size_);
}

std::expected<Value, Error> OnMemoryCache::get(const Key& key) {
    std::shared_lock lock(mutex_);
    auto it = cache_ptr_->find(key);
    if (it != cache_ptr_->end()) {
        return it->second;
    } else {
        return std::unexpected("Key not found in cache");
    }
}

void OnMemoryCache::put(const Key& key, const Value& value) {
    std::unique_lock lock(mutex_);
    cache_ptr_->insert_or_assign(key, value);
}

void OnMemoryCache::remove(const Key& key) {
    std::unique_lock lock(mutex_);
    cache_ptr_->erase(key);
}

void OnMemoryCache::clear() {
    std::unique_lock lock(mutex_);
    cache_ptr_->clear();
}

}  // namespace app::memory
