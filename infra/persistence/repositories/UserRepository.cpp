#include "infra/persistence/repositories/UserRepository.h"

#include "infra/persistence/repositories/RepositorySchema.h"

#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

User parse_user_row(const pqxx::row& row) {
    User user;
    user.id = UserId{row[0].as<std::string>()};
    user.username = row[1].as<std::string>("");
    user.display_name = row[2].as<std::string>("");
    user.avatar_seed = row[3].as<std::string>("");
    return user;
}

std::string user_select_prefix() {
    return std::string{
               "SELECT "
               "p.user_id::text, "
               "p.user_name, "
               "p.display_name, "
               "COALESCE(p.avatar_seed, '') AS avatar_seed "
               "FROM "} +
           dbschema::kProfiles + " p ";
}

}  // namespace

std::optional<std::string> UserRepository::getUserDisplayName(const UserId& userUuid) {
    return db_.read("UserRepository.getUserDisplayName",
                    [&](pqxx::work& txn) -> std::optional<std::string> {
                        const std::string query = std::string{"SELECT p.display_name FROM "} +
                                                  dbschema::kProfiles +
                                                  " p "
                                                  "WHERE p.user_id = $1::uuid "
                                                  "AND p.deleted_at IS NULL";

                        auto res = txn.exec(query, pqxx::params{userUuid.value});
                        if (res.empty()) {
                            return std::nullopt;
                        }

                        auto display_name = res[0][0].as<std::string>("");
                        if (display_name.empty()) {
                            return std::nullopt;
                        }

                        return display_name;
                    });
}

std::optional<User> UserRepository::getUser(const UserId& userUuid) {
    return db_.read("UserRepository.getUser", [&](pqxx::work& txn) -> std::optional<User> {
        const std::string query = user_select_prefix() +
                                  "WHERE p.user_id = $1::uuid "
                                  "AND p.deleted_at IS NULL";

        auto res = txn.exec(query, pqxx::params{userUuid.value});
        if (res.empty()) {
            return std::nullopt;
        }

        return parse_user_row(res[0]);
    });
}

std::vector<User> UserRepository::getUsersByIds(const std::vector<UserId>& userIds) {
    if (userIds.empty()) {
        return {};
    }

    return db_.read("UserRepository.getUsersByIds", [&](pqxx::work& txn) {
        pqxx::params params;

        std::ostringstream query;
        query << user_select_prefix() << "WHERE p.user_id = ANY(ARRAY[";

        for (size_t i = 0; i < userIds.size(); ++i) {
            if (i > 0) {
                query << ", ";
            }

            query << "$" << (i + 1) << "::uuid";
            params.append(userIds[i].value);
        }

        query << "]::uuid[]) " << "AND p.deleted_at IS NULL";

        auto res = txn.exec(query.str(), params);

        std::vector<User> users;
        users.reserve(res.size());

        for (const auto& row : res) {
            users.push_back(parse_user_row(row));
        }

        return users;
    });
}

User UserRepository::updateUserProfile(const UserId& userUuid,
                                       const std::optional<std::string>& user_name,
                                       const std::optional<std::string>& display_name,
                                       const std::optional<std::string>& avatar_seed) {
    if (!user_name.has_value() && !display_name.has_value() && !avatar_seed.has_value()) {
        auto current = getUser(userUuid);
        if (!current.has_value()) {
            throw std::runtime_error("Active user profile not found");
        }

        return *current;
    }

    return db_.write("UserRepository.updateUserProfile", [&](pqxx::work& txn) {
        std::string query =
            std::string{"UPDATE "} + dbschema::kProfiles + " SET updated_at = now()";

        pqxx::params params;
        params.append(userUuid.value);

        int next_param = 2;

        if (user_name.has_value()) {
            query += ", user_name = $" + std::to_string(next_param++) + "::text";
            params.append(*user_name);
        }

        if (display_name.has_value()) {
            query += ", display_name = $" + std::to_string(next_param++) + "::text";
            params.append(*display_name);
        }

        if (avatar_seed.has_value()) {
            query += ", avatar_seed = $" + std::to_string(next_param++) + "::text";
            params.append(*avatar_seed);
        }

        query +=
            " WHERE user_id = $1::uuid "
            "AND deleted_at IS NULL "
            "RETURNING "
            "user_id::text, "
            "user_name, "
            "display_name, "
            "COALESCE(avatar_seed, '') AS avatar_seed";

        auto res = txn.exec(query, params);
        if (res.empty()) {
            throw std::runtime_error("Active user profile not found");
        }

        return parse_user_row(res[0]);
    });
}
