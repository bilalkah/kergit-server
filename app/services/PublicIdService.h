#ifndef APP_SERVICES_PUBLICIDSERVICE_H
#define APP_SERVICES_PUBLICIDSERVICE_H

#include "domains/ids/Ids.h"

#include <optional>
#include <random>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace app::services {

class PublicIdService {
   public:
    PublicIdService();
    // Disable copy and move
    PublicIdService(const PublicIdService&) = delete;
    PublicIdService& operator=(const PublicIdService&) = delete;
    PublicIdService(PublicIdService&&) = delete;
    PublicIdService& operator=(PublicIdService&&) = delete;

    PublicHubId to_public(const HubId& internal);
    PublicChannelId to_public(const ChannelId& internal);
    PublicUserId to_public(const UserId& internal);
    PublicMessageId to_public(const MessageId& internal);

    std::optional<HubId> to_internal(const PublicHubId& external) const;
    std::optional<ChannelId> to_internal(const PublicChannelId& external) const;
    std::optional<UserId> to_internal(const PublicUserId& external) const;
    std::optional<MessageId> to_internal(const PublicMessageId& external) const;

   private:
    using ForwardMap = std::unordered_map<std::string, uint64_t>;
    using ReverseMap = std::unordered_map<uint64_t, std::string>;

    uint64_t ensure_mapping(ForwardMap& forward, ReverseMap& reverse, const std::string& key);
    std::optional<std::string> lookup_internal(const ReverseMap& reverse, uint64_t external) const;
    uint64_t generate_token();

    mutable std::shared_mutex mutex_;
    ForwardMap hub_forward_;
    ReverseMap hub_reverse_;
    ForwardMap channel_forward_;
    ReverseMap channel_reverse_;
    ForwardMap user_forward_;
    ReverseMap user_reverse_;
    ForwardMap message_forward_;
    ReverseMap message_reverse_;
    std::unordered_set<uint64_t> issued_tokens_;
    std::mt19937_64 rng_;
};

}  // namespace app::services

#endif  // APP_SERVICES_PUBLICIDSERVICE_H
