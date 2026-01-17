#include "app/services/channel/ChannelService.h"

namespace app::services {

ChannelService::ChannelService(ChannelRepository& repo)
    : repo_(repo), cache_(std::make_unique<ChannelCache>()) {}

std::optional<Channel> ChannelService::getChannel(const ChannelId& channelId) {
    // Try to get from cache first
    auto cachedChannel = cache_->get(channelId);
    if (cachedChannel) {
        return cachedChannel.value();
    }
    // Fallback to repository
    auto channelOpt = repo_.getChannel(channelId);
    if (channelOpt) {
        cache_->put(*channelOpt);
    }
    return channelOpt;
}

std::vector<Channel> ChannelService::getHubChannels(const HubId& hubId) {
    return repo_.getHubChannels(hubId);
}

ChannelId ChannelService::createChannel(const HubId& hubId, const std::string& name,
                                        const std::string& type) {
    return repo_.createChannel(hubId, name, type);
}

bool ChannelService::renameChannel(const ChannelId& channelId, const std::string& newName) {
    auto success = repo_.renameChannel(channelId, newName);
    if (success) {
        cache_->invalidate(channelId);
    }
    return success;
}
bool ChannelService::deleteChannel(const ChannelId& channelId, const HubId& hubId) {
    auto success = repo_.deleteChannel(channelId, hubId);
    if (success) {
        cache_->invalidate(channelId);
    }
    return success;
}
std::vector<Message> ChannelService::fetchMessages(const ChannelId& channelId, int limit) {
    return repo_.fetchMessages(channelId, limit);
}
std::vector<Message> ChannelService::fetchMessagesAfter(const ChannelId& channelId,
                                                        const MessageId& afterId, int limit) {
    return repo_.fetchMessagesAfter(channelId, afterId, limit);
}
std::vector<Message> ChannelService::fetchMessagesBefore(const ChannelId& channelId,
                                                         const MessageId& beforeId, int limit) {
    return repo_.fetchMessagesBefore(channelId, beforeId, limit);
}
Message ChannelService::sendMessage(const ChannelId& channelId, const UserId& senderId,
                                    const std::string& content) {
    return repo_.sendMessage(channelId, senderId, content);
}
}  // namespace app::services
