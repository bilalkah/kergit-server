#pragma once
#include <string>

enum class Authority { USER, MODERATOR, ADMIN };

class User {
public:
    std::string id;
    std::string username;
    Authority authority = Authority::USER;
    std::string current_channel;
    // Add more fields as needed (e.g., connection handle)
}; 