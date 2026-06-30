#ifndef APP_MANAGERS_SUBSCRIPTION_TOPIC_H_
#define APP_MANAGERS_SUBSCRIPTION_TOPIC_H_

#include "domains/ids/Ids.h"

#include <cstdint>
#include <functional>
#include <string>

namespace app {

enum class TopicKind : uint8_t { Hub = 0, Channel, User };

// Topic is keyed on the typed ids directly, not on a formatted "hub:<id>:channel:<id>"
// string. That avoids an fmt::format allocation on every construction (topics are built
// per fan-out / per subscribe-unsubscribe) and lets the id accessors below return the
// ids without re-parsing a string.
//   - Hub:     primary = hub id
//   - Channel: primary = hub id, secondary = channel id
//   - User:    primary = user id
struct Topic {
    TopicKind kind{TopicKind::Hub};
    std::string primary;
    std::string secondary;

    bool operator==(const Topic& other) const noexcept {
        return kind == other.kind && primary == other.primary && secondary == other.secondary;
    }

    static Topic HubTopic(const HubId& hub) { return {TopicKind::Hub, hub.value, {}}; }

    static Topic ChannelTopic(const HubId& hub, const ChannelId& channel) {
        return {TopicKind::Channel, hub.value, channel.value};
    }

    static Topic UserTopic(const UserId& user) { return {TopicKind::User, user.value, {}}; }
};

namespace topic_utils {

inline HubId extractHubId(const Topic& topic) {
    if (topic.kind == TopicKind::Hub || topic.kind == TopicKind::Channel) {
        return HubId{topic.primary};
    }
    return {};
}

inline ChannelId extractChannelId(const Topic& topic) {
    return topic.kind == TopicKind::Channel ? ChannelId{topic.secondary} : ChannelId{};
}

inline UserId extractUserId(const Topic& topic) {
    return topic.kind == TopicKind::User ? UserId{topic.primary} : UserId{};
}

}  // namespace topic_utils

}  // namespace app

namespace std {
template <>
struct hash<app::Topic> {
    size_t operator()(const app::Topic& t) const noexcept {
        size_t h = static_cast<size_t>(t.kind);
        h ^= std::hash<std::string>{}(t.primary) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        h ^= std::hash<std::string>{}(t.secondary) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        return h;
    }
};
}  // namespace std

#endif  // APP_MANAGERS_SUBSCRIPTION_TOPIC_H_
