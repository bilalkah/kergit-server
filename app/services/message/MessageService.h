#ifndef APP_SERVICES_MESSAGE_MESSAGESERVICE_H
#define APP_SERVICES_MESSAGE_MESSAGESERVICE_H

#include "infra/persistence/repositories/MessageRepository.h"

#include <chrono>
#include <expected>
#include <memory>
#include <vector>

namespace app::services {

class AsyncMessageWriter;

class MessageService {
   public:
    explicit MessageService(MessageRepository& repo);
    ~MessageService();

    enum class MessageError {
        RepoFailure,
        QueueFull,
    };

    std::expected<std::vector<Message>, MessageError> fetchMessages(const ChannelId& channelId,
                                                                    int limit);
    std::expected<std::vector<Message>, MessageError> fetchMessagesAfter(const ChannelId& channelId,
                                                                         const MessageCursor& after,
                                                                         int limit);
    std::expected<std::vector<Message>, MessageError> fetchMessagesBefore(
        const ChannelId& channelId, const MessageCursor& before, int limit);
    std::expected<Message, MessageError> sendMessage(const ChannelId& channelId,
                                                     const UserId& senderId,
                                                     const std::string& content);

    void startAsyncWriter(std::size_t capacity, std::size_t max_retries,
                          std::chrono::milliseconds retry_delay);
    void stopAsyncWriter();

   private:
    MessageRepository& repo_;
    std::unique_ptr<AsyncMessageWriter> async_writer_;
};

}  // namespace app::services

#endif  // APP_SERVICES_MESSAGE_MESSAGESERVICE_H
