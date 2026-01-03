#ifndef DOMAINS_HUB_H
#define DOMAINS_HUB_H

#include "domains/ids/Ids.h"

#include <string>
#include <unordered_map>
#include <unordered_set>

enum class Role { USER, ADMIN, OWNER };

struct UserIdHash {
    size_t operator()(const UserId& u) const noexcept { return std::hash<std::string>{}(u.value); }
};

struct UserIdEq {
    bool operator()(const UserId& a, const UserId& b) const noexcept { return a.value == b.value; }
};

struct Hub {
    Hub() {}
    Hub(std::string hub_name, HubId hub_id, UserId owner_id)
        : name(std::move(hub_name)), id(std::move(hub_id)), owner(std::move(owner_id)) {
        members.emplace(owner, Role::OWNER);
    }

    void setMemberRole(const UserId& uid, Role role) { members.insert_or_assign(uid, role); }

    std::string name{""};
    HubId id{""};
    UserId owner{""};

    std::unordered_map<UserId, Role, UserIdHash, UserIdEq> members{};
};

#endif  // DOMAINS_HUB_H