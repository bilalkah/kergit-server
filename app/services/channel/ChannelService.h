#ifndef APP_SERVICES_CHANNEL_CHANNELSERVICE_H
#define APP_SERVICES_CHANNEL_CHANNELSERVICE_H

#include "app/services/channel/ChannelCache.h"
#include "infra/persistence/repositories/ChannelRepository.h"

#include <memory>
#include <optional>

namespace app::services {

class ChannelService {
   public:
    explicit ChannelService(ChannelRepository& repo);

    // ---- Channel reads ----
    std::optional<Channel> getChannel(const ChannelId& channelId);
    std::vector<Channel> getHubChannels(const HubId& hubId);
    // ---- Channel writes ----
    ChannelId createChannel(const HubId& hubId, const std::string& name, const std::string& type);
    bool renameChannel(const ChannelId& channelId, const std::string& newName);
    bool deleteChannel(const ChannelId& channelId, const HubId& hubId);

    // ---- Message reads ----
    std::vector<Message> fetchMessages(const ChannelId& channelId, int limit);
    std::vector<Message> fetchMessagesAfter(const ChannelId& channelId, const MessageId& afterId,
                                            int limit);
    std::vector<Message> fetchMessagesBefore(const ChannelId& channelId, const MessageId& beforeId,
                                             int limit);
    // ---- Message writes ----
    Message sendMessage(const ChannelId& channelId, const UserId& senderId,
                        const std::string& content);

   private:
    ChannelRepository& repo_;
    std::unique_ptr<IChannelCache> cache_;
};

}  // namespace app::services

#endif
