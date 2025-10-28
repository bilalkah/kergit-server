#pragma once

#include <algorithm>
#include <utility>
#include <string>
#include <unordered_set>
#include <vector>

enum class Role { USER, ADMIN, OWNER };

class Hub {
   public:
    std::string name;
    std::unordered_set<std::string> channel_ids;
    std::unordered_map<std::string, Role> user_ids;
    std::string owner;

    Hub(const std::string& hub_name, const std::string& owner_id)
        : name(hub_name), owner(owner_id) {
        user_ids.insert(std::make_pair(owner_id, Role::OWNER));
    }

    bool set_owner(const std::string& owner_id, Role role) {
        if (!user_exists(owner_id)) {
            return false;  // not exists
        }
        owner = owner_id;
        user_ids.insert(std::make_pair(owner_id, role));
        return true;
    }

    bool add_user(const std::string& user_id, Role role) {
        if (user_exists(user_id)) {
            return false;  // already exists
        }
        user_ids.insert(std::make_pair(user_id, role));
        return true;
    }

    bool remove_user(const std::string& user_id) {
        if (!user_exists(user_id)) {
            return false;  // not exists
        }
        user_ids.erase(user_id);
        return true;
    }

    bool create_channel(const std::string& channel_id) {
        if (channel_exists(channel_id)) {
            return false;  // already exists
        }
        channel_ids.insert(channel_id);
        return true;
    }

    bool remove_channel(const std::string& channel_id) {
        if (!channel_exists(channel_id)) {
            return false;  // not exists
        }
        channel_ids.erase(channel_id);
        return true;
    }

   private:
    bool user_exists(const std::string& user_id) const {
        return std::find_if(user_ids.begin(), user_ids.end(),
                            [&](const std::pair<std::string, Role>& p) {
                                return p.first == user_id;
                            }) != user_ids.end();
    }

    bool channel_exists(const std::string& channel_id) const {
        return channel_ids.find(channel_id) != channel_ids.end();
    }
};