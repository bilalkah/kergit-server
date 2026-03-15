#ifndef APP_SERVICES_USERSERVICE_H
#define APP_SERVICES_USERSERVICE_H

#include "app/services/user/UserCache.h"
#include "domains/User.h"
#include "infra/persistence/repositories/UserRepository.h"

#include <expected>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace app::services {

class UserService {
   public:
    UserService(UserRepository& repo);

    // ---- Read operations ----
    // Returns full user aggregate (cache-backed)
    std::optional<User> getUser(const UserId& userId);
    // Cache-first bulk user read. Misses are fetched via a single repository query.
    std::unordered_map<UserId, User> getUsersByIds(const std::vector<UserId>& userIds);
    // Returns display name only (cheap path, no cache fill)
    std::optional<std::string> getDisplayName(const UserId& userId);

    // ---- Write operations ----
    // Updates username / full_name and invalidates cache
    enum class UpdateError {
        RepoFailure,
    };
    std::expected<void, UpdateError> updateProfile(const UserId& userId,
                                                   const std::optional<std::string>& username,
                                                   const std::optional<std::string>& full_name);
    std::expected<void, UpdateError> updateSettings(const UserId& userId,
                                                    const std::optional<std::string>& username,
                                                    const std::optional<std::string>& avatar_seed);

   private:
    UserRepository& repo_;
    std::unique_ptr<IUserCache> cache_;
};

}  // namespace app::services

#endif
