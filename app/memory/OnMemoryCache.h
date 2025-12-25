#ifndef APP_MEMORY_ONMEMORYCACHE_CPP
#define APP_MEMORY_ONMEMORYCACHE_CPP

#include "app/memory/ICache.h"

#include <memory>
#include <shared_mutex>
#include <unordered_map>

namespace app::memory {
class OnMemoryCache : public ICache {
   public:
    OnMemoryCache();
    std::expected<Value, Error> get(const Key& key) override;
    void put(const Key& key, const Value& value) override;
    void remove(const Key& key) override;
    void clear() override;

   private:
    size_t expected_size_{1024};
    std::unique_ptr<std::unordered_map<Key, Value>> cache_ptr_;
    mutable std::shared_mutex mutex_;
};

}  // namespace app::memory

#endif  // APP_MEMORY_ONMEMORYCACHE_CPP
