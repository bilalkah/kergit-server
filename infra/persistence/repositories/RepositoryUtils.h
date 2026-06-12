#ifndef INFRA_PERSISTENCE_REPOSITORY_UTILS_H
#define INFRA_PERSISTENCE_REPOSITORY_UTILS_H

#include "domains/Channel.h"
#include "domains/Hub.h"
#include "domains/Message.h"

#include <algorithm>
#include <cctype>
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

#endif  // INFRA_PERSISTENCE_REPOSITORY_UTILS_H
