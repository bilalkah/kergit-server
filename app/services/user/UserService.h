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
    // Updates app-visible profile fields and refreshes the cache.
    //
    // user_name:     unique account handle (profiles.user_name)
    // display_name:  visible profile name (profiles.display_name)
    // avatar_seed:   visible avatar seed (profiles.avatar_seed)
    //
    // Only fields with a value are changed. Returns the updated user.
    enum class UpdateError {
        RepoFailure,
        // user_name unique constraint violated (handle already taken).
        DuplicateUsername,
    };
    std::expected<User, UpdateError> updateProfile(const UserId& userId,
                                                   const std::optional<std::string>& user_name,
                                                   const std::optional<std::string>& display_name,
                                                   const std::optional<std::string>& avatar_seed);

   private:
    UserRepository& repo_;
    std::unique_ptr<IUserCache> cache_;
};

}  // namespace app::services

#endif
