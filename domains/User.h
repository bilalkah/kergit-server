#ifndef DOMAINS_USER_H
#define DOMAINS_USER_H

#include "domains/ids/Ids.h"

#include <chrono>
#include <string>

struct UserPreferences {
    bool notifications_enabled = true;
    bool sound_enabled = true;
    std::string preferred_language = "en";
};

struct User {
    User() {}
    User(UserId uid, std::string uname, std::string dname, std::string mail)
        : id(std::move(uid)),
          username(std::move(uname)),
          full_name(std::move(dname)),
          email(std::move(mail)) {}

    UserId id{""};
    std::string username{};
    std::string full_name{};
    std::string email{};
    std::string avatar_seed{};
    UserPreferences prefs{};
};

#endif  // DOMAINS_USER_H
