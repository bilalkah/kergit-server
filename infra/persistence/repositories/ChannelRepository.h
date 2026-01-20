#ifndef INFRA_PERSISTENCE_REPOSITORIES_CHANNEL_REPOSITORY_H
#define INFRA_PERSISTENCE_REPOSITORIES_CHANNEL_REPOSITORY_H

#include "domains/Channel.h"
#include "domains/Message.h"
#include "infra/persistence/DatabaseExecutor.h"

#include <optional>
#include <string>
#include <vector>

class ChannelRepository {
   public:
    explicit ChannelRepository(DatabaseExecutor& db) : db_(db) {}

    ChannelId createChannel(const HubId& hubId, const std::string& channelName,
                            const std::string& type);
    bool deleteChannel(const ChannelId& channelId, const HubId& hubId);
    Message sendMessage(const ChannelId& channelId, const UserId& senderUuid,
                        const std::string& content);
    std::vector<Message> fetchMessages(const ChannelId& channelId, int limit);
    std::vector<Message> fetchMessagesAfter(const ChannelId& channelId, const MessageId& afterId,
                                            int limit);
    std::vector<Message> fetchMessagesBefore(const ChannelId& channelId, const MessageId& beforeId,
                                             int limit);
    // Preferred for hot paths: single query for initial/after/before pagination.
    std::vector<Message> fetchMessagesPage(const ChannelId& channelId,
                                           std::optional<MessageId> after,
                                           std::optional<MessageId> before, int limit);
    std::vector<Channel> getHubChannels(const HubId& hubId);
    std::optional<Channel> getChannel(const ChannelId& channelId);
    bool renameChannel(const ChannelId& channelId, const std::string& name);

   private:
    DatabaseExecutor& db_;
};

#endif  // INFRA_PERSISTENCE_REPOSITORIES_CHANNEL_REPOSITORY_H
