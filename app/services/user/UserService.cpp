#include "app/services/user/UserService.h"

namespace app::services {

UserService::UserService(UserRepository& repo)
    : repo_(repo), cache_(std::make_unique<UserCache>()) {}

std::optional<User> UserService::getUser(const UserId& userId) {
    // Try to get from cache first
    auto cachedUser = cache_->get(userId);
    if (cachedUser) {
        return cachedUser.value();
    }
    // Fallback to repository
    auto userOpt = repo_.getUser(userId);
    if (userOpt) {
        cache_->put(*userOpt);
    }
    return userOpt;
}

std::optional<std::string> UserService::getDisplayName(const UserId& userId) {
    auto user = getUser(userId);  // cache-backed
    if (!user) return std::nullopt;
    return user->username;
}

void UserService::updateProfile(const UserId& userId, const std::optional<std::string>& username,
                                const std::optional<std::string>& full_name) {
    repo_.updateUserProfile(userId, username, full_name);
    cache_->invalidate(userId);
}
}  // namespace app::services
