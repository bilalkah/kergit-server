#pragma once
#include <string>
#include <ctime>

class Message {
public:
    std::string sender;
    std::string text;
    std::time_t timestamp;
}; 