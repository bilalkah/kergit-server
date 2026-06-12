#ifndef INFRA_PERSISTENCE_REPOSITORIES_USER_REPOSITORY_H
#define INFRA_PERSISTENCE_REPOSITORIES_USER_REPOSITORY_H

#include "domains/User.h"
#include "domains/ids/Ids.h"
#include "infra/persistence/DatabaseExecutor.h"

#include <optional>
#include <string>
#include <tuple>
#include <vector>

class UserRepository {
   public:
    explicit UserRepository(DatabaseExecutor& db) : db_(db) {}

    std::optional<User> getUser(const UserId& userUuid);
    std::vector<User> getUsersByIds(const std::vector<UserId>& userIds);
    std::optional<std::string> getUserDisplayName(const UserId& userUuid);

    // Updates app-visible profile fields.
    //
    // user_name:
    // - unique handle
    // - lowercase/formatted by DB constraint
    // - used for username/account settings
    //
    // display_name:
    // - non-unique visible name
    // - used by Profile tab and message/member display
    //
    // avatar_seed:
    // - visible avatar seed
    //
    // Call examples:
    // - Profile tab:
    //   updateUserProfile(user_id, nullopt, display_name, avatar_seed)
    //
    // - Account username change:
    //   updateUserProfile(user_id, user_name, nullopt, nullopt)
    User updateUserProfile(const UserId& userUuid, const std::optional<std::string>& user_name,
                           const std::optional<std::string>& display_name,
                           const std::optional<std::string>& avatar_seed);

   private:
    DatabaseExecutor& db_;
};

#endif  // INFRA_PERSISTENCE_REPOSITORIES_USER_REPOSITORY_H
