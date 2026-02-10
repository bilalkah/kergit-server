#include "app/services/channel/ChannelService.h"

#include "app/services/hub/HubService.h"
#include "utils/Logger.h"

namespace app::services {

ChannelService::ChannelService(ChannelRepository& repo)
    : repo_(repo), cache_(std::make_unique<ChannelCache>()) {}

void ChannelService::setHubService(HubService& hub_service) { hub_service_ = &hub_service; }

std::optional<Channel> ChannelService::getChannel(const ChannelId& channelId) {
    if (hub_service_) {
        if (auto hit = hub_service_->tryGetSnapshotChannel(channelId)) {
            const auto& [hub_id, ch] = *hit;
            // utils::log_line(utils::LogLevel::INFO,
            //                 "channel_snapshot hit channel_id=" + channelId.value +
            //                     " hub_id=" + hub_id.value);
            Channel channel{ch.name, ch.id, hub_id, ch.type};
            cache_->put(channel);
            return channel;
        }

        // utils::log_line(utils::LogLevel::INFO,
        //                 "channel_snapshot miss channel_id=" + channelId.value + " build=true");

        // No snapshot yet; use cached hub_id if available to build the snapshot.
        auto cachedChannel = cache_->get(channelId);
        if (cachedChannel) {
            const auto snapshot = hub_service_->getOrBuildSnapshot(cachedChannel->hub_id);
            for (const auto& ch : snapshot.channels) {
                if (ch.id == channelId) {
                    Channel channel{ch.name, ch.id, snapshot.id, ch.type};
                    cache_->put(channel);
                    return channel;
                }
            }
            return std::nullopt;
        }

        // Snapshot requires hub_id; fall back to DB lookup to discover owning hub.
        auto channelOpt = repo_.getChannel(channelId);
        if (!channelOpt) return std::nullopt;
        const auto snapshot = hub_service_->getOrBuildSnapshot(channelOpt->hub_id);
        for (const auto& ch : snapshot.channels) {
            if (ch.id == channelId) {
                Channel channel{ch.name, ch.id, snapshot.id, ch.type};
                cache_->put(channel);
                return channel;
            }
        }
        return std::nullopt;
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
    if (hub_service_) {
        if (auto cached = hub_service_->tryGetSnapshot(hubId)) {
            // utils::log_line(utils::LogLevel::INFO,
            //                 "channel_snapshot hit hub_id=" + hubId.value);
            std::vector<Channel> chans;
            chans.reserve(cached->channels.size());
            for (const auto& ch : cached->channels) {
                chans.emplace_back(ch.name, ch.id, hubId, ch.type);
            }
            return chans;
        }
        // utils::log_line(utils::LogLevel::INFO,
        //                 "channel_snapshot miss hub_id=" + hubId.value + " build=true");
        const auto snapshot = hub_service_->getOrBuildSnapshot(hubId);
        std::vector<Channel> chans;
        chans.reserve(snapshot.channels.size());
        for (const auto& ch : snapshot.channels) {
            chans.emplace_back(ch.name, ch.id, hubId, ch.type);
        }
        return chans;
    }

    // utils::log_line(utils::LogLevel::INFO,
    //                 "channel_snapshot fallback hub_id=" + hubId.value + " source=db");
    // No HubService wired; fall back to DB read for hub channels.
    return repo_.getHubChannels(hubId);
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
        return repo_.sendMessage(channelId, senderId, content);
    } catch (...) {
        return std::unexpected(MessageError::RepoFailure);
    }
}
}  // namespace app::services
