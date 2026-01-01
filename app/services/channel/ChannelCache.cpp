#include "app/services/channel/ChannelCache.h"

namespace app::services {
ChannelCache::ChannelCache() : cache_(std::make_unique<core::cache::OnMemoryCache>()) {}

std::expected<Channel, ChannelCacheError> ChannelCache::get(ChannelId id) {
    auto key = core::cache::AnyKey::make(id);

    auto res = cache_->get(key);
    if (!res) {
        return std::unexpected(ChannelCacheError::NotFound);
    }

    auto* channel = std::any_cast<Channel>(&res.value());
    if (!channel) {
        cache_->erase(key);
        return std::unexpected(ChannelCacheError::CorruptedEntry);
    }
    return *channel;
}

void ChannelCache::put(const Channel& channel) {
    auto key = core::cache::AnyKey::make(channel.id);
    cache_->put(key, channel);
}

void ChannelCache::invalidate(ChannelId id) {
    auto key = core::cache::AnyKey::make(id);
    cache_->erase(key);
}

}  // namespace app::services
