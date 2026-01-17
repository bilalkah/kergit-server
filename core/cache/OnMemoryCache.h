#ifndef CORE_CACHE_ONMEMORYCACHE_H
#define CORE_CACHE_ONMEMORYCACHE_H

#include "core/cache/ICache.h"

#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace core::cache {

class OnMemoryCache final : public ICache {
   public:
    bool contains(const AnyKey& key) const override;
    std::expected<std::any, CacheError> get(const AnyKey& key) override;
    void put(const AnyKey& key, std::any value) override;
    void erase(const AnyKey& key) override;
    void clear() override;

    bool update(const AnyKey& key, const std::function<void(std::any&)>& fn) override;

    std::size_t size() const override;

   private:
    mutable std::shared_mutex mu_;
    std::unordered_map<AnyKey, std::any> map_;
};

}  // namespace core::cache

#endif  // CORE_CACHE_ONMEMORYCACHE_H
