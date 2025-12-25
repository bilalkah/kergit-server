#ifndef APP_MEMORY_ICACHE_H
#define APP_MEMORY_ICACHE_H

#include "domains/Channel.h"
#include "domains/Hub.h"
#include "domains/User.h"
#include "domains/ids/Ids.h"

#include <expected>
#include <variant>

namespace app::memory {

using Key = std::variant<HubId, ChannelId, UserId>;
using Value = std::variant<Hub, Channel, User>;
using Error = std::string;

class ICache {
   public:
    virtual ~ICache() = default;
    virtual std::expected<Value, Error> get(const Key& key) = 0;
    virtual void put(const Key& key, const Value& value) = 0;
    virtual void remove(const Key& key) = 0;
    virtual void clear() = 0;
};
}  // namespace app::memory

#endif  // APP_MEMORY_ICACHE_H
