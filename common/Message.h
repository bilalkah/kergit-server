#pragma once
#include <ctime>
#include <string>

class Message {
   public:
    std::string sender;
    std::string text;
    std::time_t timestamp;
};