#pragma once
#include <string>
#include <unordered_set>
#include <vector>
#include "Message.h"

class Channel {
public:
    std::string name;
    std::unordered_set<std::string> user_ids; // or User*
    std::vector<Message> history;
    std::string owner_id;
    bool is_persistent = true;
    // Methods: addUser, removeUser, addMessage, etc. can be added later
}; 