#ifndef DOMAINS_HUB_H
#define DOMAINS_HUB_H

#pragma once
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

class Hub {
   public:
    std::string name{};
    HubId id{""};
    UserId owner{""};

    std::unordered_map<UserId, Role, UserIdHash, UserIdEq> members{};

    Hub() = default;

    Hub(std::string hub_name, HubId hub_id, UserId owner_id)
        : name(std::move(hub_name)), id(std::move(hub_id)), owner(std::move(owner_id)) {
        members.emplace(owner, Role::OWNER);
    }

    Hub(const Hub&) = default;

    bool hasMember(const UserId& uid) const { return members.find(uid) != members.end(); }

    bool setMemberRole(const UserId& uid, Role role) {
        auto it = members.find(uid);
        if (it == members.end()) {
            members.emplace(uid, role);
            return true;
        }
        if (it->second != role) {
            it->second = role;
            return true;
        }
        return false;
    }

    bool removeMember(const UserId& uid) {
        if (uid.value == owner.value) return false;
        return members.erase(uid) > 0;
    }

    bool is_initialized() const noexcept { return !id.value.empty() && !owner.value.empty(); }
};

#endif  // DOMAINS_HUB_H