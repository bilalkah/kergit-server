#ifndef DOMAINS_USER_H
#define DOMAINS_USER_H

#include "domains/ids/Ids.h"

#include <string>
#include <utility>

struct UserPreferences {
    bool notifications_enabled = true;
    bool sound_enabled = true;
    std::string preferred_language = "en";
};

struct User {
    User() = default;

    User(UserId uid, std::string uname, std::string dname, std::string avatar = {})
        : id(std::move(uid)),
          username(std::move(uname)),
          display_name(std::move(dname)),
          avatar_seed(std::move(avatar)) {}

    UserId id{};

    // Unique account handle.
    // Stored as kergit_app.profiles.user_name.
    // Used for account username settings, future mentions/search/profile lookup.
    std::string username{};

    // Non-unique visible profile name.
    // Stored as kergit_app.profiles.display_name.
    // Used in profile UI, member list, messages, and deleted-user display.
    std::string display_name{};

    // Visible avatar seed.
    // Stored as kergit_app.profiles.avatar_seed.
    std::string avatar_seed{};

    UserPreferences prefs{};
};

#endif  // DOMAINS_USER_H
