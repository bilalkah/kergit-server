#include "app/services/invite/InviteService.h"

#include <chrono>
#include <random>

namespace app::services {

namespace {

constexpr auto INVITE_TTL = std::chrono::seconds(3600);
constexpr auto MIN_REUSE_TTL = std::chrono::seconds(180);  // reuse if ≥3 min remaining
constexpr std::string_view REDIS_KEY_PREFIX = "invite:";
constexpr std::string_view REDIS_HUB_KEY_PREFIX = "invite_hub:";
constexpr std::string_view BASE62_CHARS =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
constexpr std::size_t TOKEN_LENGTH = 10;

}  // namespace

InviteService::InviteService(infra::redis::RedisClient& redis, const std::string& base_url)
    : redis_(redis), base_url_(base_url) {}

std::string InviteService::generate_token() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<std::size_t> dist(0, BASE62_CHARS.size() - 1);

    std::string token;
    token.reserve(TOKEN_LENGTH);
    for (std::size_t i = 0; i < TOKEN_LENGTH; ++i) {
        token += BASE62_CHARS[dist(gen)];
    }
    return token;
}

std::string InviteService::createInvite(const HubId& hub_id) {
    // Check for existing valid token for this hub
    std::string hub_key = std::string(REDIS_HUB_KEY_PREFIX) + hub_id.value;
    auto existing_token = redis_.get(hub_key);
    if (existing_token.has_value()) {
        std::string token_key = std::string(REDIS_KEY_PREFIX) + *existing_token;
        auto remaining = redis_.ttl(token_key);
        if (remaining.has_value() && *remaining >= MIN_REUSE_TTL) {
            return base_url_ + "/" + *existing_token;
        }
    }

    // Generate new token and store both forward + reverse lookup keys
    auto token = generate_token();
    std::string key = std::string(REDIS_KEY_PREFIX) + token;
    redis_.setex(key, INVITE_TTL, hub_id.value);
    redis_.setex(hub_key, INVITE_TTL, token);
    return base_url_ + "/" + token;
}

std::optional<HubId> InviteService::resolveInvite(const std::string& token) {
    std::string key = std::string(REDIS_KEY_PREFIX) + token;
    auto value = redis_.get(key);
    if (!value.has_value()) return std::nullopt;
    return HubId{*value};
}

void InviteService::revokeInvitesForHub(const HubId& hub_id) {
    // There is at most one active token per hub (tracked by the reverse key);
    // deleting both makes every previously shared link for this hub invalid.
    std::string hub_key = std::string(REDIS_HUB_KEY_PREFIX) + hub_id.value;
    auto existing_token = redis_.get(hub_key);
    if (existing_token.has_value()) {
        redis_.del(std::string(REDIS_KEY_PREFIX) + *existing_token);
    }
    redis_.del(hub_key);
}

}  // namespace app::services
