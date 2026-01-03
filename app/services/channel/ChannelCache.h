#ifndef APP_SERVICES_CHANNEL_CHANNELCACHE_H
#define APP_SERVICES_CHANNEL_CHANNELCACHE_H

#include "core/cache/OnMemoryCache.h"
#include "domains/Channel.h"

#include <expected>
#include <memory>

namespace app::services {

enum class ChannelCacheError { NotFound, CorruptedEntry };

class IChannelCache {
   public:
    virtual ~IChannelCache() = default;

    virtual std::expected<Channel, ChannelCacheError> get(ChannelId id) = 0;

    virtual void put(const Channel& channel) = 0;
    virtual void invalidate(ChannelId id) = 0;
};

class ChannelCache final : public IChannelCache {
   public:
    explicit ChannelCache();
    ~ChannelCache() override = default;

    std::expected<Channel, ChannelCacheError> get(ChannelId id) override;
    void put(const Channel& channel) override;

    void invalidate(ChannelId id) override;

   private:
    std::unique_ptr<core::cache::OnMemoryCache> cache_;
};

}  // namespace app::services

#endif  // APP_SERVICES_CHANNEL_CHANNELCACHE_H
