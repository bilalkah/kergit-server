#ifndef INFRA_PERSISTENCE_REPOSITORIES_CHANNEL_REPOSITORY_H
#define INFRA_PERSISTENCE_REPOSITORIES_CHANNEL_REPOSITORY_H

#include "domains/Channel.h"
#include "domains/Message.h"
#include "infra/persistence/RepositoryMux.h"

#include <optional>
#include <string>
#include <vector>

class ChannelRepository {
   public:
    explicit ChannelRepository(RepositoryMux& mux) : mux_(mux) {}

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
    std::vector<Channel> getHubChannels(const HubId& hubId);
    std::optional<Channel> getChannel(const ChannelId& channelId);
    bool renameChannel(const ChannelId& channelId, const std::string& name);

   private:
    RepositoryMux& mux_;
};

#endif  // INFRA_PERSISTENCE_REPOSITORIES_CHANNEL_REPOSITORY_H
