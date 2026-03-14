#include "app/services/channel/ChannelService.h"

#include "app/services/channel/AsyncMessageWriter.h"
#include "app/services/hub/HubService.h"
#include "utils/Logger.h"
#include "utils/Uuid.h"

namespace app::services {

ChannelService::ChannelService(ChannelRepository& repo)
    : repo_(repo), cache_(std::make_unique<ChannelCache>()) {}

ChannelService::~ChannelService() = default;

void ChannelService::setHubService(HubService& hub_service) { hub_service_ = &hub_service; }

std::optional<Channel> ChannelService::getChannel(const ChannelId& channelId) {
    if (hub_service_) {
        if (auto hit = hub_service_->tryGetSnapshotChannel(channelId)) {
            const auto& [hub_id, ch] = *hit;

            std::string channel_name;
            if (auto cached = cache_->get(channelId)) {
                channel_name = cached->name;
            } else if (auto from_repo = repo_.getChannel(channelId)) {
                channel_name = from_repo->name;
            } else {
                return std::nullopt;
            }

            Channel channel{std::move(channel_name), ch.id, hub_id, ch.type};
            cache_->put(channel);
            return channel;
        }

        auto cachedChannel = cache_->get(channelId);
        if (cachedChannel) {
            const auto snapshot = hub_service_->getOrBuildSnapshot(cachedChannel->hub_id);
            for (const auto& ch : snapshot.channels) {
                if (ch.id == channelId) {
                    Channel channel{cachedChannel->name, ch.id, snapshot.id, ch.type};
                    cache_->put(channel);
                    return channel;
                }
            }
        }

        auto channelOpt = repo_.getChannel(channelId);
        if (!channelOpt) return std::nullopt;
        const auto snapshot = hub_service_->getOrBuildSnapshot(channelOpt->hub_id);
        for (const auto& ch : snapshot.channels) {
            if (ch.id == channelId) {
                Channel channel{channelOpt->name, ch.id, snapshot.id, ch.type};
                cache_->put(channel);
                return channel;
            }
        }

        // Snapshot may lag on corner races; return repository result as fallback.
        cache_->put(*channelOpt);
        return channelOpt;
    }

    // utils::log_line(utils::LogLevel::INFO,
    //                 "channel_snapshot fallback channel_id=" + channelId.value + " source=db");
    auto cachedChannel = cache_->get(channelId);
    if (cachedChannel) {
        return cachedChannel.value();
    }
    // No HubService wired; fall back to DB read for channel metadata.
    auto channelOpt = repo_.getChannel(channelId);
    if (channelOpt) {
        cache_->put(*channelOpt);
    }
    return channelOpt;
}

std::vector<Channel> ChannelService::getHubChannels(const HubId& hubId) {
    // Channel names are mutable and intentionally not cached in HubSnapshot.
    // Always read current names from repository for channel listings.
    auto channels = repo_.getHubChannels(hubId);
    for (const auto& channel : channels) {
        cache_->put(channel);
    }
    return channels;
}

ChannelId ChannelService::createChannel(const HubId& hubId, const std::string& name,
                                        const std::string& type) {
    auto id = repo_.createChannel(hubId, name, type);
    if (hub_service_) {
        hub_service_->invalidateSnapshot(hubId);
    }
    return id;
}

bool ChannelService::renameChannel(const ChannelId& channelId, const std::string& newName) {
    auto success = repo_.renameChannel(channelId, newName);
    if (success) {
        cache_->invalidate(channelId);
        if (hub_service_) {
            hub_service_->invalidateSnapshotsForChannel(channelId);
        }
    }
    return success;
}
bool ChannelService::deleteChannel(const ChannelId& channelId, const HubId& hubId) {
    auto success = repo_.deleteChannel(channelId, hubId);
    if (success) {
        cache_->invalidate(channelId);
        if (hub_service_) {
            hub_service_->invalidateSnapshot(hubId);
        }
    }
    return success;
}
std::expected<std::vector<Message>, ChannelService::MessageError> ChannelService::fetchMessages(
    const ChannelId& channelId, int limit) {
    try {
        return repo_.fetchMessages(channelId, limit);
    } catch (...) {
        return std::unexpected(MessageError::RepoFailure);
    }
}
std::expected<std::vector<Message>, ChannelService::MessageError>
ChannelService::fetchMessagesAfter(const ChannelId& channelId, const MessageId& afterId,
                                   int limit) {
    try {
        return repo_.fetchMessagesAfter(channelId, afterId, limit);
    } catch (...) {
        return std::unexpected(MessageError::RepoFailure);
    }
}
std::expected<std::vector<Message>, ChannelService::MessageError>
ChannelService::fetchMessagesBefore(const ChannelId& channelId, const MessageId& beforeId,
                                    int limit) {
    try {
        return repo_.fetchMessagesBefore(channelId, beforeId, limit);
    } catch (...) {
        return std::unexpected(MessageError::RepoFailure);
    }
}
std::expected<Message, ChannelService::MessageError> ChannelService::sendMessage(
    const ChannelId& channelId, const UserId& senderId, const std::string& content) {
    try {
        if (async_writer_) {
            Message msg;
            msg.id = MessageId{utils::generate_uuid_v4()};
            msg.ch_id = channelId;
            msg.sender_id = senderId;
            msg.text = content;
            msg.sent_at = std::chrono::system_clock::now();

            if (!async_writer_->enqueue(msg)) {
                return std::unexpected(MessageError::QueueFull);
            }
            return msg;
        }
        return repo_.sendMessage(channelId, senderId, content);
    } catch (...) {
        return std::unexpected(MessageError::RepoFailure);
    }
}

void ChannelService::startAsyncWriter(std::size_t capacity, std::size_t max_retries,
                                      std::chrono::milliseconds retry_delay) {
    if (async_writer_) return;
    async_writer_ = std::make_unique<AsyncMessageWriter>(repo_, capacity, max_retries, retry_delay);
    async_writer_->start();
}

void ChannelService::stopAsyncWriter() {
    if (!async_writer_) return;
    async_writer_->stop();
    async_writer_.reset();
}
}  // namespace app::services
