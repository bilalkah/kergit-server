#ifndef CORE_CACHE_ICACHE_H
#define CORE_CACHE_ICACHE_H

#include "core/cache/AnyKey.h"

#include <any>
#include <expected>
#include <functional>

namespace core::cache {

using CacheError = std::string;

class ICache {
   public:
    virtual ~ICache() = default;

    virtual bool contains(const AnyKey& key) const = 0;
    virtual std::expected<std::any, CacheError> get(const AnyKey& key) = 0;
    virtual void put(const AnyKey& key, std::any value) = 0;
    virtual void erase(const AnyKey& key) = 0;
    virtual void clear() = 0;

    virtual bool update(const AnyKey& key, const std::function<void(std::any&)>& fn) = 0;

    virtual std::size_t size() const = 0;
};
}  // namespace core::cache

#endif  // CORE_CACHE_ICACHE_H
