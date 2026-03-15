#include "app/services/message/MessageService.h"

#include "app/services/message/AsyncMessageWriter.h"
#include "utils/Logger.h"
#include "utils/Uuid.h"

#include <chrono>
#include <sstream>

namespace app::services {

namespace {

long long elapsed_ms(const std::chrono::steady_clock::time_point start) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                                 start)
        .count();
}

uint64_t now_unix_us() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto us = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
    return us > 0 ? static_cast<uint64_t>(us) : 0;
}

void log_message_service(const std::string& op, const std::string& details) {
    (void)op;
    (void)details;
}

}  // namespace

MessageService::MessageService(MessageRepository& repo) : repo_(repo) {}

MessageService::~MessageService() = default;

std::expected<std::vector<Message>, MessageService::MessageError> MessageService::fetchMessages(
    const ChannelId& channelId, int limit) {
    const auto started_at = std::chrono::steady_clock::now();
    try {
        auto messages = repo_.fetchMessages(channelId, limit);
        std::ostringstream details;
        details << "channel_id=" << channelId.value << " limit=" << limit << " db=1 fetched="
                << messages.size() << " total_ms=" << elapsed_ms(started_at);
        log_message_service("fetchMessages", details.str());
        return messages;
    } catch (...) {
        std::ostringstream details;
        details << "channel_id=" << channelId.value << " limit=" << limit
                << " db=1 error=1 total_ms=" << elapsed_ms(started_at);
        log_message_service("fetchMessages", details.str());
        return std::unexpected(MessageError::RepoFailure);
    }
}

std::expected<std::vector<Message>, MessageService::MessageError> MessageService::fetchMessagesAfter(
    const ChannelId& channelId, const MessageCursor& after, int limit) {
    const auto started_at = std::chrono::steady_clock::now();
    try {
        auto messages = repo_.fetchMessagesAfter(channelId, after, limit);
        std::ostringstream details;
        details << "channel_id=" << channelId.value << " after_id=" << after.message_id.value
                << " after_us=" << after.created_at_unix_us
                << " limit=" << limit << " db=1 fetched=" << messages.size()
                << " total_ms=" << elapsed_ms(started_at);
        log_message_service("fetchMessagesAfter", details.str());
        return messages;
    } catch (...) {
        std::ostringstream details;
        details << "channel_id=" << channelId.value << " after_id=" << after.message_id.value
                << " after_us=" << after.created_at_unix_us
                << " limit=" << limit << " db=1 error=1 total_ms=" << elapsed_ms(started_at);
        log_message_service("fetchMessagesAfter", details.str());
        return std::unexpected(MessageError::RepoFailure);
    }
}

std::expected<std::vector<Message>, MessageService::MessageError>
MessageService::fetchMessagesBefore(const ChannelId& channelId, const MessageCursor& before,
                                    int limit) {
    const auto started_at = std::chrono::steady_clock::now();
    try {
        auto messages = repo_.fetchMessagesBefore(channelId, before, limit);
        std::ostringstream details;
        details << "channel_id=" << channelId.value << " before_id=" << before.message_id.value
                << " before_us=" << before.created_at_unix_us
                << " limit=" << limit << " db=1 fetched=" << messages.size()
                << " total_ms=" << elapsed_ms(started_at);
        log_message_service("fetchMessagesBefore", details.str());
        return messages;
    } catch (...) {
        std::ostringstream details;
        details << "channel_id=" << channelId.value << " before_id=" << before.message_id.value
                << " before_us=" << before.created_at_unix_us
                << " limit=" << limit << " db=1 error=1 total_ms=" << elapsed_ms(started_at);
        log_message_service("fetchMessagesBefore", details.str());
        return std::unexpected(MessageError::RepoFailure);
    }
}

std::expected<Message, MessageService::MessageError> MessageService::sendMessage(
    const ChannelId& channelId, const UserId& senderId, const std::string& content) {
    const auto started_at = std::chrono::steady_clock::now();
    try {
        if (async_writer_) {
            Message msg;
            msg.id = MessageId{utils::generate_uuid_v4()};
            msg.ch_id = channelId;
            msg.sender_id = senderId;
            msg.text = content;
            msg.created_at_unix_us = now_unix_us();

            if (!async_writer_->enqueue(msg)) {
                std::ostringstream details;
                details << "channel_id=" << channelId.value << " sender_id=" << senderId.value
                        << " async=1 queued=0 total_ms=" << elapsed_ms(started_at);
                log_message_service("sendMessage", details.str());
                return std::unexpected(MessageError::QueueFull);
            }
            std::ostringstream details;
            details << "channel_id=" << channelId.value << " sender_id=" << senderId.value
                    << " async=1 queued=1 total_ms=" << elapsed_ms(started_at);
            log_message_service("sendMessage", details.str());
            return msg;
        }
        auto msg = repo_.sendMessage(channelId, senderId, content);
        std::ostringstream details;
        details << "channel_id=" << channelId.value << " sender_id=" << senderId.value
                << " async=0 db=1 total_ms=" << elapsed_ms(started_at);
        log_message_service("sendMessage", details.str());
        return msg;
    } catch (...) {
        std::ostringstream details;
        details << "channel_id=" << channelId.value << " sender_id=" << senderId.value
                << " error=1 total_ms=" << elapsed_ms(started_at);
        log_message_service("sendMessage", details.str());
        return std::unexpected(MessageError::RepoFailure);
    }
}

void MessageService::startAsyncWriter(std::size_t capacity, std::size_t max_retries,
                                      std::chrono::milliseconds retry_delay) {
    if (async_writer_) return;
    async_writer_ = std::make_unique<AsyncMessageWriter>(repo_, capacity, max_retries, retry_delay);
    async_writer_->start();
}

void MessageService::stopAsyncWriter() {
    if (!async_writer_) return;
    async_writer_->stop();
    async_writer_.reset();
}

}  // namespace app::services
