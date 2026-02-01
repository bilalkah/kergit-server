#include "app/services/presence/PresenceService.h"

#include <unordered_set>

namespace app::services {

std::vector<UserId> PresenceService::onlineUsers() const { return sessions_.activeUsers(); }

std::vector<UserId> PresenceService::onlineUsersInHub(const HubId& hub) const {
    auto exp = subs_.getSubscribers(Topic::HubTopic(hub));
    if (!exp) {
        return {};
    }
    const auto& subscribers = exp;
    std::unordered_set<UserId> unique;
    std::vector<UserId> online;
    for (const auto& conn : *subscribers) {
        auto user_exp = sessions_.sessionOfConnection(conn);
        if (!user_exp.has_value()) continue;
        if (unique.insert(user_exp.value()).second) {
            online.push_back(user_exp.value());
        }
    }
    return online;
}

std::vector<UserId> PresenceService::onlineUsersInChannel(const HubId& hub,
                                                          const ChannelId& channel) const {
    auto exp = subs_.getSubscribers(Topic::ChannelTopic(hub, channel));
    if (!exp) {
        return {};
    }
    const auto& subscribers = exp;
    std::unordered_set<UserId> unique;
    std::vector<UserId> online;
    for (const auto& conn : *subscribers) {
        auto user_exp = sessions_.sessionOfConnection(conn);
        if (!user_exp.has_value()) continue;
        if (unique.insert(user_exp.value()).second) {
            online.push_back(user_exp.value());
        }
    }
    return online;
}

bool PresenceService::isUserOnline(const UserId& user) const { return sessions_.hasSession(user); }

}  // namespace app::services
