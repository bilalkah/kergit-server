#include "app/services/PublicIdService.h"

#include <array>
#include <chrono>
#include <mutex>
#include <utility>

namespace app::services {

namespace {
constexpr std::array<char, 62> kAlphabet = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
    'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V',
    'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l',
    'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'};
constexpr std::size_t kAlphabetSize = kAlphabet.size();
constexpr std::size_t kTokenLength = 22;
}  // namespace

PublicIdService::PublicIdService() {
    std::random_device rd;
    std::seed_seq seq{rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd()};
    rng_.seed(seq);
}

PublicHubId PublicIdService::to_public(const HubId& internal) {
    if (internal.value.empty()) return {};
    {
        std::shared_lock lock(mutex_);
        auto it = hub_forward_.find(internal.value);
        if (it != hub_forward_.end()) return PublicHubId{it->second};
    }
    std::unique_lock lock(mutex_);
    return PublicHubId{ensure_mapping(hub_forward_, hub_reverse_, internal.value)};
}

PublicChannelId PublicIdService::to_public(const ChannelId& internal) {
    if (internal.value.empty()) return {};
    {
        std::shared_lock lock(mutex_);
        auto it = channel_forward_.find(internal.value);
        if (it != channel_forward_.end()) return PublicChannelId{it->second};
    }
    std::unique_lock lock(mutex_);
    return PublicChannelId{ensure_mapping(channel_forward_, channel_reverse_, internal.value)};
}

PublicUserId PublicIdService::to_public(const UserId& internal) {
    if (internal.value.empty()) return {};
    {
        std::shared_lock lock(mutex_);
        auto it = user_forward_.find(internal.value);
        if (it != user_forward_.end()) return PublicUserId{it->second};
    }
    std::unique_lock lock(mutex_);
    return PublicUserId{ensure_mapping(user_forward_, user_reverse_, internal.value)};
}

PublicMessageId PublicIdService::to_public(const MessageId& internal) {
    if (internal.value.empty()) return {};
    {
        std::shared_lock lock(mutex_);
        auto it = message_forward_.find(internal.value);
        if (it != message_forward_.end()) return PublicMessageId{it->second};
    }
    std::unique_lock lock(mutex_);
    return PublicMessageId{ensure_mapping(message_forward_, message_reverse_, internal.value)};
}

std::optional<HubId> PublicIdService::to_internal(const PublicHubId& external) const {
    if (external.value.empty()) return std::nullopt;
    std::shared_lock lock(mutex_);
    auto raw = lookup_internal(hub_reverse_, external.value);
    if (!raw) return std::nullopt;
    return HubId{*raw};
}

std::optional<ChannelId> PublicIdService::to_internal(const PublicChannelId& external) const {
    if (external.value.empty()) return std::nullopt;
    std::shared_lock lock(mutex_);
    auto raw = lookup_internal(channel_reverse_, external.value);
    if (!raw) return std::nullopt;
    return ChannelId{*raw};
}

std::optional<UserId> PublicIdService::to_internal(const PublicUserId& external) const {
    if (external.value.empty()) return std::nullopt;
    std::shared_lock lock(mutex_);
    auto raw = lookup_internal(user_reverse_, external.value);
    if (!raw) return std::nullopt;
    return UserId{*raw};
}

std::optional<MessageId> PublicIdService::to_internal(const PublicMessageId& external) const {
    if (external.value.empty()) return std::nullopt;
    std::shared_lock lock(mutex_);
    auto raw = lookup_internal(message_reverse_, external.value);
    if (!raw) return std::nullopt;
    return MessageId{*raw};
}

std::string PublicIdService::ensure_mapping(ForwardMap& forward, ReverseMap& reverse,
                                            const std::string& key) {
    auto it = forward.find(key);
    if (it != forward.end()) return it->second;

    std::string token;
    do {
        token = generate_token();
    } while (issued_tokens_.count(token) != 0U || reverse.find(token) != reverse.end());

    forward.emplace(key, token);
    reverse.emplace(token, key);
    issued_tokens_.insert(token);
    return token;
}

std::optional<std::string> PublicIdService::lookup_internal(const ReverseMap& reverse,
                                                            const std::string& external) const {
    auto it = reverse.find(external);
    if (it == reverse.end()) return std::nullopt;
    return it->second;
}

std::string PublicIdService::generate_token() {
    std::uniform_int_distribution<std::size_t> dist(0, kAlphabetSize - 1);
    std::string token;
    token.reserve(kTokenLength);
    for (std::size_t i = 0; i < kTokenLength; ++i) {
        token.push_back(kAlphabet[dist(rng_)]);
    }
    return token;
}

}  // namespace app::services
