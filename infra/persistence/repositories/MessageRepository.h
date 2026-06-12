#ifndef INFRA_PERSISTENCE_REPOSITORIES_MESSAGE_REPOSITORY_H
#define INFRA_PERSISTENCE_REPOSITORIES_MESSAGE_REPOSITORY_H

#include "domains/Message.h"
#include "infra/persistence/DatabaseExecutor.h"

#include <optional>
#include <vector>

class MessageRepository {
   public:
    explicit MessageRepository(DatabaseExecutor& db) : db_(db) {}

    Message sendMessage(const ChannelId& channelId, const UserId& senderUuid,
                        const std::string& content,
                        std::vector<MessageAttachment> attachments = {},
                        std::optional<MessageLinkPreview> link_preview = std::nullopt);
    bool insertMessage(const Message& msg);
    std::vector<Message> fetchMessages(const ChannelId& channelId, int limit);
    std::vector<Message> fetchMessagesAfter(const ChannelId& channelId, const MessageCursor& after,
                                            int limit);
    std::vector<Message> fetchMessagesBefore(const ChannelId& channelId, const MessageCursor& before,
                                             int limit);
    std::vector<Message> fetchMessagesPage(const ChannelId& channelId,
                                           std::optional<MessageCursor> after,
                                           std::optional<MessageCursor> before, int limit);

   private:
    DatabaseExecutor& db_;
};

#endif  // INFRA_PERSISTENCE_REPOSITORIES_MESSAGE_REPOSITORY_H
