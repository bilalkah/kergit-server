#ifndef INFRA_PERSISTENCE_REPOSITORIES_USER_REPOSITORY_H
#define INFRA_PERSISTENCE_REPOSITORIES_USER_REPOSITORY_H

#include "domains/User.h"
#include "domains/ids/Ids.h"
#include "infra/persistence/DatabaseExecutor.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

class UserRepository {
   public:
    explicit UserRepository(DatabaseExecutor& db) : db_(db) {}

    std::optional<User> getUser(const UserId& userUuid);
    std::vector<User> getUsersByIds(const std::vector<UserId>& userIds);
    std::optional<std::string> getUserDisplayName(const UserId& userUuid);
    std::pair<std::string, std::string> updateUserProfile(
        const UserId& userUuid, const std::optional<std::string>& username,
        const std::optional<std::string>& full_name);
    void updateUserSettings(const UserId& userUuid, const std::optional<std::string>& username,
                            const std::optional<std::string>& avatar_seed);

   private:
    DatabaseExecutor& db_;
};

#endif  // INFRA_PERSISTENCE_REPOSITORIES_USER_REPOSITORY_H
