#include "app/services/presence/PresenceService.h"

namespace app::services {

std::vector<UserId> PresenceService::onlineUsers() const { return sessions_.activeUsers(); }

std::vector<UserId> PresenceService::onlineUsersInHub(const HubId& hub) const {
    auto exp = subs_.getSubscribers(Topic::HubTopic(hub));
    if (!exp.has_value()) {
        return {};
    }
    const auto& subscribers = exp.value();
    std::vector<UserId> online;
    for (const auto& user : subscribers) {
        if (sessions_.hasSession(user)) {
            online.push_back(user);
        }
    }
    return online;
}

std::vector<UserId> PresenceService::onlineUsersInChannel(const HubId& hub,
                                                          const ChannelId& channel) const {
    auto exp = subs_.getSubscribers(Topic::ChannelTopic(hub, channel));
    if (!exp.has_value()) {
        return {};
    }
    const auto& subscribers = exp.value();
    std::vector<UserId> online;
    for (const auto& user : subscribers) {
        if (sessions_.hasSession(user)) {
            online.push_back(user);
        }
    }
    return online;
}

bool PresenceService::isUserOnline(const UserId& user) const { return sessions_.hasSession(user); }

}  // namespace app::services
