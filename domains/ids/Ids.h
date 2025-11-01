// domain/ids/Ids.h
#pragma once
#include <functional>
#include <string>

struct HubId {
    std::string value;
    HubId(std::string v) : value(std::move(v)) {}
};
struct ChannelId {
    std::string value;
    ChannelId(std::string v) : value(std::move(v)) {}
};
struct UserId {
    std::string value;
    UserId(std::string v) : value(std::move(v)) {}
};
struct MessageId {
    std::string value;
    MessageId(std::string v) : value(std::move(v)) {}
};

struct ConnId {
    std::string value;
    ConnId(std::string v) : value(std::move(v)) {}
};

inline bool operator==(const HubId& a, const HubId& b) { return a.value == b.value; }
inline bool operator==(const ChannelId& a, const ChannelId& b) { return a.value == b.value; }
inline bool operator==(const UserId& a, const UserId& b) { return a.value == b.value; }
inline bool operator==(const MessageId& a, const MessageId& b) { return a.value == b.value; }
inline bool operator==(const ConnId& a, const ConnId& b) { return a.value == b.value; }

namespace std {
template <>
struct hash<HubId> {
    size_t operator()(const HubId& x) const noexcept { return std::hash<std::string>{}(x.value); }
};
template <>
struct hash<ChannelId> {
    size_t operator()(const ChannelId& x) const noexcept {
        return std::hash<std::string>{}(x.value);
    }
};
template <>
struct hash<UserId> {
    size_t operator()(const UserId& x) const noexcept { return std::hash<std::string>{}(x.value); }
};
template <>
struct hash<ConnId> {
    size_t operator()(const ConnId& x) const noexcept { return std::hash<std::string>{}(x.value); }
};
}  // namespace std
