#ifndef INFRA_PERSISTENCE_REPOSITORY_UTILS_H
#define INFRA_PERSISTENCE_REPOSITORY_UTILS_H

#include "domains/Channel.h"
#include "domains/Hub.h"
#include "domains/Message.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>

inline Role role_from_string(const std::string& role) {
    std::string lower = role;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower == "owner") return Role::OWNER;
    if (lower == "admin") return Role::ADMIN;
    return Role::USER;
}

inline ChannelType channel_type_from_string(const std::string& type) {
    std::string lower = type;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lower == "voice" ? ChannelType::VOICE : ChannelType::CHAT;
}

inline std::chrono::system_clock::time_point parse_timestamp(const std::string& ts) {
    if (ts.empty()) return {};
    std::string trimmed = ts;
    auto dot = trimmed.find('.');
    if (dot != std::string::npos) {
        trimmed = trimmed.substr(0, dot);
    }
    std::tm tm{};
    std::istringstream ss(trimmed);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (ss.fail()) return {};
    tm.tm_isdst = -1;
    std::time_t time = std::mktime(&tm);
    if (time == static_cast<std::time_t>(-1)) return {};
    return std::chrono::system_clock::from_time_t(time);
}

#endif  // INFRA_PERSISTENCE_REPOSITORY_UTILS_H
