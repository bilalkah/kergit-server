#include "app/services/user/UserService.h"

#include "utils/Logger.h"

#include <chrono>
#include <sstream>
#include <unordered_set>

namespace app::services {

namespace {

long long elapsed_ms(const std::chrono::steady_clock::time_point start) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                                 start)
        .count();
}

void log_user_cache(const std::string& op, const std::string& details) {
    (void)op;
    (void)details;
}

}  // namespace

UserService::UserService(UserRepository& repo)
    : repo_(repo), cache_(std::make_unique<UserCache>()) {}

std::optional<User> UserService::getUser(const UserId& userId) {
    const auto started_at = std::chrono::steady_clock::now();
    // Try to get from cache first
    auto cachedUser = cache_->get(userId);
    if (cachedUser) {
        std::ostringstream details;
        details << "user_id=" << userId.value << " hit=1 db=0 found=1 total_ms="
                << elapsed_ms(started_at);
        log_user_cache("getUser", details.str());
        return cachedUser.value();
    }
    // Fallback to repository
    const auto db_started_at = std::chrono::steady_clock::now();
    auto userOpt = repo_.getUser(userId);
    const auto db_ms = elapsed_ms(db_started_at);
    if (userOpt) {
        cache_->put(*userOpt);
    }
    std::ostringstream details;
    details << "user_id=" << userId.value << " hit=0 db=1 db_ms=" << db_ms
            << " found=" << (userOpt.has_value() ? 1 : 0) << " total_ms=" << elapsed_ms(started_at);
    log_user_cache("getUser", details.str());
    return userOpt;
}

std::unordered_map<UserId, User> UserService::getUsersByIds(const std::vector<UserId>& userIds) {
    const auto started_at = std::chrono::steady_clock::now();
    std::unordered_map<UserId, User> users;
    if (userIds.empty()) {
        log_user_cache("getUsersByIds", "input=0 unique=0 cache_hits=0 db=0 total_ms=0");
        return users;
    }

    users.reserve(userIds.size());
    std::unordered_set<UserId> seen_ids;
    seen_ids.reserve(userIds.size());
    std::vector<UserId> misses;
    misses.reserve(userIds.size());
    size_t cache_hits = 0;

    for (const auto& user_id : userIds) {
        if (user_id.value.empty()) {
            continue;
        }
        if (!seen_ids.insert(user_id).second) {
            continue;
        }

        auto cached_user = cache_->get(user_id);
        if (cached_user) {
            users.insert_or_assign(user_id, cached_user.value());
            ++cache_hits;
            continue;
        }
        misses.push_back(user_id);
    }

    if (misses.empty()) {
        std::ostringstream details;
        details << "input=" << userIds.size() << " unique=" << seen_ids.size()
                << " cache_hits=" << cache_hits << " db=0 total_ms=" << elapsed_ms(started_at);
        log_user_cache("getUsersByIds", details.str());
        return users;
    }

    const auto db_started_at = std::chrono::steady_clock::now();
    const auto fetched_users = repo_.getUsersByIds(misses);
    const auto db_ms = elapsed_ms(db_started_at);
    for (const auto& user : fetched_users) {
        cache_->put(user);
        users.insert_or_assign(user.id, user);
    }

    std::ostringstream details;
    details << "input=" << userIds.size() << " unique=" << seen_ids.size()
            << " cache_hits=" << cache_hits << " misses=" << misses.size() << " db=1 db_ms="
            << db_ms << " fetched=" << fetched_users.size() << " total_ms="
            << elapsed_ms(started_at);
    log_user_cache("getUsersByIds", details.str());

    return users;
}

std::optional<std::string> UserService::getDisplayName(const UserId& userId) {
    auto user = getUser(userId);  // cache-backed
    if (!user) return std::nullopt;
    return user->username;
}

std::expected<void, UserService::UpdateError> UserService::updateProfile(
    const UserId& userId, const std::optional<std::string>& username,
    const std::optional<std::string>& full_name) {
    try {
        repo_.updateUserProfile(userId, username, full_name);
    } catch (...) {
        return std::unexpected(UpdateError::RepoFailure);
    }
    cache_->invalidate(userId);
    return {};
}

std::expected<void, UserService::UpdateError> UserService::updateSettings(
    const UserId& userId, const std::optional<std::string>& username,
    const std::optional<std::string>& avatar_seed) {
    try {
        repo_.updateUserSettings(userId, username, avatar_seed);
    } catch (...) {
        return std::unexpected(UpdateError::RepoFailure);
    }
    cache_->invalidate(userId);
    return {};
}
}  // namespace app::services
