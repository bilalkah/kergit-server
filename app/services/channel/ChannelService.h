#ifndef APP_SERVICES_CHANNEL_CHANNELSERVICE_H
#define APP_SERVICES_CHANNEL_CHANNELSERVICE_H

#include "app/services/channel/ChannelCache.h"
#include "infra/persistence/repositories/ChannelRepository.h"

#include <expected>
#include <memory>
#include <optional>

namespace app::services {

class HubService;

class ChannelService {
   public:
    explicit ChannelService(ChannelRepository& repo);
    void setHubService(HubService& hub_service);

    // ---- Channel reads ----
    std::optional<Channel> getChannel(const ChannelId& channelId);
    std::vector<Channel> getHubChannels(const HubId& hubId);
    // ---- Channel writes ----
    ChannelId createChannel(const HubId& hubId, const std::string& name, const std::string& type);
    bool renameChannel(const ChannelId& channelId, const std::string& newName);
    bool deleteChannel(const ChannelId& channelId, const HubId& hubId);

    // ---- Message reads ----
    enum class MessageError {
        RepoFailure,
    };
    std::expected<std::vector<Message>, MessageError> fetchMessages(const ChannelId& channelId,
                                                                    int limit);
    std::expected<std::vector<Message>, MessageError> fetchMessagesAfter(
        const ChannelId& channelId, const MessageId& afterId, int limit);
    std::expected<std::vector<Message>, MessageError> fetchMessagesBefore(
        const ChannelId& channelId, const MessageId& beforeId, int limit);
    // ---- Message writes ----
    std::expected<Message, MessageError> sendMessage(const ChannelId& channelId,
                                                     const UserId& senderId,
                                                     const std::string& content);

   private:
    ChannelRepository& repo_;
    std::unique_ptr<IChannelCache> cache_;
    HubService* hub_service_{nullptr};
};

}  // namespace app::services

#endif
