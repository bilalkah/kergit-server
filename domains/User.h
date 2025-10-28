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

class User {
   public:
    UserId id{""};
    std::string username{};
    std::string full_name{};
    std::string email{};
    UserPreferences prefs{};
    ChannelId current_channel{""};

    std::chrono::system_clock::time_point created_at{};
    std::chrono::system_clock::time_point updated_at{};

    User() = default;
    User(UserId uid, std::string uname, std::string dname, std::string mail)
        : id(std::move(uid)),
          username(std::move(uname)),
          full_name(std::move(dname)),
          email(std::move(mail)) {}

    void channelJoin(const ChannelId& channel_id) { current_channel = channel_id; }
    void channelLeave() { current_channel = ChannelId{""}; }

    // optional helper
    bool is_initialized() const noexcept { return !id.value.empty() && !username.empty(); }
};

#endif  // DOMAINS_USER_H