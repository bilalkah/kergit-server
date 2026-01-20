#ifndef INFRA_PERSISTENCE_REPOSITORIES_USER_REPOSITORY_H
#define INFRA_PERSISTENCE_REPOSITORIES_USER_REPOSITORY_H

#include "domains/User.h"
#include "domains/ids/Ids.h"
#include "infra/persistence/DatabaseExecutor.h"

#include <optional>
#include <string>
#include <utility>

class UserRepository {
   public:
    explicit UserRepository(DatabaseExecutor& db) : db_(db) {}

    std::optional<User> getUser(const UserId& userUuid);
    std::optional<std::string> getUserDisplayName(const UserId& userUuid);
    std::pair<std::string, std::string> updateUserProfile(
        const UserId& userUuid, const std::optional<std::string>& username,
        const std::optional<std::string>& full_name);

   private:
    DatabaseExecutor& db_;
};

#endif  // INFRA_PERSISTENCE_REPOSITORIES_USER_REPOSITORY_H
