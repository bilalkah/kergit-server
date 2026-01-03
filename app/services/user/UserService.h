#ifndef APP_SERVICES_USERSERVICE_H
#define APP_SERVICES_USERSERVICE_H

#include "app/services/user/UserCache.h"
#include "domains/User.h"
#include "infra/persistence/repositories/UserRepository.h"

#include <optional>
#include <string_view>

namespace app::services {

class UserService {
   public:
    UserService(UserRepository& repo);

    // ---- Read operations ----
    // Returns full user aggregate (cache-backed)
    std::optional<User> getUser(const UserId& userId);
    // Returns display name only (cheap path, no cache fill)
    std::optional<std::string> getDisplayName(const UserId& userId);

    // ---- Write operations ----
    // Updates username / full_name and invalidates cache
    void updateProfile(const UserId& userId, const std::optional<std::string>& username,
                       const std::optional<std::string>& full_name);

   private:
    UserRepository& repo_;
    std::unique_ptr<IUserCache> cache_;
};

}  // namespace app::services

#endif
