#ifndef APP_SERVICES_HUB_CHANNELCACHE_H
#define APP_SERVICES_HUB_CHANNELCACHE_H

#include "domains/Channel.h"
#include "domains/ids/Ids.h"

#include <mutex>
#include <optional>
#include <shared_mutex>
#include <unordered_map>

namespace app::services {

// Thread-safe in-memory cache of Channel records keyed by ChannelId.
//
// Single responsibility: own the channel-by-id cache tier (storage + locking) so HubService can
// orchestrate repository + cache without also holding an ad-hoc map and its own mutex inline.
class ChannelCache {
   public:
    std::optional<Channel> get(const ChannelId& channelId) const {
        std::shared_lock lock(mutex_);
        const auto it = cache_.find(channelId);
        if (it == cache_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    void put(const Channel& channel) {
        std::unique_lock lock(mutex_);
        cache_.insert_or_assign(channel.id, channel);
    }

    void invalidate(const ChannelId& channelId) {
        std::unique_lock lock(mutex_);
        cache_.erase(channelId);
    }

   private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<ChannelId, Channel> cache_;
};

}  // namespace app::services

#endif  // APP_SERVICES_HUB_CHANNELCACHE_H
