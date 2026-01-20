#include "infra/persistence/repositories/UserRepository.h"

#include <nlohmann/json.hpp>
#include <stdexcept>

std::optional<std::string> UserRepository::getUserDisplayName(const UserId& userUuid) {
    return db_.read("UserRepository.getUserDisplayName",
                    [&](pqxx::work& txn) -> std::optional<std::string> {
        auto res = txn.exec(
            "SELECT COALESCE(raw_user_meta_data->>'username', "
            "raw_user_meta_data->>'preferred_username',"
            " raw_user_meta_data->>'full_name', '') AS display FROM auth.users WHERE id = $1::uuid",
            pqxx::params{userUuid.value});
        if (res.empty()) return std::nullopt;
        auto display = res[0][0].as<std::string>();
        if (display.empty()) return std::nullopt;
        return display;
    });
}

std::optional<User> UserRepository::getUser(const UserId& userUuid) {
    return db_.read("UserRepository.getUser", [&](pqxx::work& txn) -> std::optional<User> {
        auto res = txn.exec(
            "SELECT u.id, COALESCE(u.raw_user_meta_data::text, '{}'), "
            "COALESCE(p.avatar_seed, '') "
            "FROM auth.users u "
            "LEFT JOIN public.profiles p ON p.user_id = u.id "
            "WHERE u.id = $1::uuid",
            pqxx::params{userUuid.value});
        if (res.empty()) return std::nullopt;

        nlohmann::json meta;
        try {
            meta = nlohmann::json::parse(res[0][1].as<std::string>(), nullptr, false);
            if (!meta.is_object()) meta = nlohmann::json::object();
        } catch (...) {
            meta = nlohmann::json::object();
        }

        User user;
        user.id = UserId{res[0][0].as<std::string>()};
        user.username = meta.value("username", std::string{});
        user.full_name = meta.value("full_name", std::string{});
        user.email = meta.value("email", std::string{});
        user.avatar_seed = res[0][2].as<std::string>("");

        return user;
    });
}

std::pair<std::string, std::string> UserRepository::updateUserProfile(
    const UserId& userUuid, const std::optional<std::string>& username,
    const std::optional<std::string>& full_name) {
    if (!username.has_value() && !full_name.has_value()) return {std::string{}, std::string{}};

    return db_.write("UserRepository.updateUserProfile", [&](pqxx::work& txn) {
        auto res = txn.exec(
            "SELECT COALESCE(raw_user_meta_data::text, '{}') FROM auth.users WHERE id = $1::uuid "
            "FOR UPDATE",
            pqxx::params{userUuid.value});
        if (res.empty()) throw std::runtime_error("User not found");

        nlohmann::json meta;
        try {
            meta = nlohmann::json::parse(res[0][0].as<std::string>(), nullptr, false);
            if (!meta.is_object()) meta = nlohmann::json::object();
        } catch (...) {
            meta = nlohmann::json::object();
        }

        if (username.has_value()) meta["username"] = *username;
        if (full_name.has_value()) meta["full_name"] = *full_name;

        const std::string payload = meta.dump();
        txn.exec("UPDATE auth.users SET raw_user_meta_data = $2::jsonb WHERE id = $1::uuid",
                 pqxx::params{userUuid.value, payload});

        std::string final_username = meta.value("username", std::string{});
        std::string final_full_name = meta.value("full_name", std::string{});
        return std::make_pair(std::move(final_username), std::move(final_full_name));
    });
}
