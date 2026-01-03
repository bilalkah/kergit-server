#include "app/services/user/UserCache.h"

namespace app::services {

UserCache::UserCache() : cache_(std::make_unique<core::cache::OnMemoryCache>()) {}

std::expected<User, UserCacheError> UserCache::get(UserId id) {
    auto key = core::cache::AnyKey::make(id);
    auto res = cache_->get(key);
    if (!res) {
        return std::unexpected(UserCacheError::NotFound);
    }

    auto* user = std::any_cast<User>(&res.value());
    if (!user) {
        cache_->erase(key);
        return std::unexpected(UserCacheError::CorruptedEntry);
    }
    return *user;
}

void UserCache::put(const User& user) {
    cache_->put(core::cache::AnyKey::make(user.id), user);
}

void UserCache::invalidate(UserId id) {
    cache_->erase(core::cache::AnyKey::make(id));
}

}  // namespace app::services
