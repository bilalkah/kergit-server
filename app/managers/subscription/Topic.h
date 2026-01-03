#ifndef APP_MANAGERS_SUBSCRIPTION_TOPIC_H_
#define APP_MANAGERS_SUBSCRIPTION_TOPIC_H_

#include "domains/ids/Ids.h"

#include <cstdint>
#include <fmt/format.h>
#include <functional>
#include <string>

namespace app {

enum class TopicKind : uint8_t { Hub = 0, Channel, User };

struct Topic {
    TopicKind kind;
    std::string topic_id;

    bool operator==(const Topic& other) const noexcept {
        return kind == other.kind && topic_id == other.topic_id;
    }

    static Topic HubTopic(const HubId& hub) {
        return {TopicKind::Hub, fmt::format("hub:{}", hub.value)};
    }

    static Topic ChannelTopic(const HubId& hub, const ChannelId& channel) {
        return {TopicKind::Channel, fmt::format("hub:{}:channel:{}", hub.value, channel.value)};
    }

    static Topic UserTopic(const UserId& user) {
        return {TopicKind::User, fmt::format("user:{}", user.value)};
    }
};

namespace topic_utils {

inline HubId extractHubId(const Topic& topic) {
    // Expect: hub:<hubId> or hub:<hubId>:channel:<channelId>
    if (topic.kind != TopicKind::Hub) return {};

    const std::string& s = topic.topic_id;

    constexpr std::string_view hubPrefix = "hub:";
    constexpr std::string_view channelPart = ":channel:";

    auto start = hubPrefix.size();
    auto end = (topic.kind == TopicKind::Channel) ? s.find(channelPart) : s.size();

    return HubId{s.substr(start, end - start)};
}

inline ChannelId extractChannelId(const Topic& topic) {
    // Expect: hub:<hubId>:channel:<channelId>
    if (topic.kind != TopicKind::Channel) return {};

    constexpr std::string_view kChannelPart = ":channel:";

    const std::string& s = topic.topic_id;
    auto pos = s.find(kChannelPart);
    if (pos == std::string::npos) return {};

    return ChannelId{s.substr(pos + kChannelPart.size())};
}

inline UserId extractUserId(const Topic& topic) {
    // Expect: user:<userId>
    if (topic.kind != TopicKind::User) return {};

    constexpr std::string_view kUserPrefix = "user:";

    const std::string& s = topic.topic_id;
    if (!s.starts_with(kUserPrefix)) return {};

    return UserId{s.substr(kUserPrefix.size())};
}

}  // namespace topic_utils

}  // namespace app

namespace std {
template <>
struct hash<app::Topic> {
    size_t operator()(const app::Topic& t) const noexcept {
        size_t h = static_cast<size_t>(t.kind);
        h ^= std::hash<std::string>{}(t.topic_id) << 1;
        return h;
    }
};
}  // namespace std

#endif  // APP_MANAGERS_SUBSCRIPTION_TOPIC_H_
