#ifndef DOMAINS_IDS_IDS_H_
#define DOMAINS_IDS_IDS_H_

#include <functional>
#include <string>

struct HubId {
    std::string value;
    HubId() = default;
    HubId(std::string v) : value(std::move(v)) {}
};
struct ChannelId {
    std::string value;
    ChannelId() = default;
    ChannelId(std::string v) : value(std::move(v)) {}
};
struct UserId {
    std::string value;
    UserId() = default;
    UserId(std::string v) : value(std::move(v)) {}
};
struct MessageId {
    std::string value;
    MessageId() = default;
    MessageId(std::string v) : value(std::move(v)) {}
};

struct ConnId {
    std::string value;
    ConnId() = default;
    ConnId(std::string v) : value(std::move(v)) {}
};

struct NetStackId {
    std::string value;
    NetStackId() = default;
    NetStackId(std::string v) : value(std::move(v)) {}
};

struct GlobalConnId {
    NetStackId netstack_id;
    ConnId conn_id;
    GlobalConnId() = default;
    GlobalConnId(NetStackId nsid, ConnId cid)
        : netstack_id(std::move(nsid)), conn_id(std::move(cid)) {}
};

inline bool operator==(const HubId& a, const HubId& b) { return a.value == b.value; }
inline bool operator==(const ChannelId& a, const ChannelId& b) { return a.value == b.value; }
inline bool operator==(const UserId& a, const UserId& b) { return a.value == b.value; }
inline bool operator==(const MessageId& a, const MessageId& b) { return a.value == b.value; }
inline bool operator==(const ConnId& a, const ConnId& b) { return a.value == b.value; }
inline bool operator==(const NetStackId& a, const NetStackId& b) { return a.value == b.value; }
inline bool operator==(const GlobalConnId& a, const GlobalConnId& b) {
    return a.netstack_id.value == b.netstack_id.value && a.conn_id.value == b.conn_id.value;
}

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
template <>
struct hash<NetStackId> {
    size_t operator()(const NetStackId& x) const noexcept {
        return std::hash<std::string>{}(x.value);
    }
};
template <>
struct hash<GlobalConnId> {
    size_t operator()(const GlobalConnId& x) const noexcept {
        size_t h1 = std::hash<std::string>{}(x.netstack_id.value);
        size_t h2 = std::hash<std::string>{}(x.conn_id.value);
        return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
    }
};
}  // namespace std

#endif  // DOMAINS_IDS_IDS_H_
