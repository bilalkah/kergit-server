#ifndef INFRA_PERSISTENCE_REPOSITORIES_USER_REPOSITORY_H
#define INFRA_PERSISTENCE_REPOSITORIES_USER_REPOSITORY_H

#include "domains/User.h"
#include "domains/ids/Ids.h"
#include "infra/persistence/RepositoryMux.h"

#include <optional>
#include <string>
#include <utility>

class UserRepository {
   public:
    explicit UserRepository(RepositoryMux& mux) : mux_(mux) {}

    std::optional<User> getUser(const UserId& userUuid);
    std::optional<std::string> getUserDisplayName(const UserId& userUuid);
    std::pair<std::string, std::string> updateUserProfile(
        const UserId& userUuid, const std::optional<std::string>& username,
        const std::optional<std::string>& full_name);

   private:
    RepositoryMux& mux_;
};

#endif  // INFRA_PERSISTENCE_REPOSITORIES_USER_REPOSITORY_H
