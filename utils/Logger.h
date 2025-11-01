#ifndef UTILS_LOGGER_H
#define UTILS_LOGGER_H

#include <ctime>
#include <iomanip>
#include <iostream>
#include <string>

namespace utils {
enum class LogLevel { INFO, WARNING, CRITICAL };

inline std::string timestamp() {
    std::time_t t = std::time(nullptr);
    std::tm tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%H:%M:%S");
    return oss.str();
}

inline const char* level_to_string(LogLevel lvl) {
    switch (lvl) {
        case LogLevel::INFO:
            return "INFO";
        case LogLevel::WARNING:
            return "WARN";
        case LogLevel::CRITICAL:
            return "CRIT";
        default:
            return "LOG";
    }
}

inline void log_line(LogLevel lvl, const std::string& msg) {
    std::cout << "[" << timestamp() << "] " << "[" << level_to_string(lvl) << "] " << msg
              << std::endl;
}

};  // namespace utils

#endif  // UTILS_LOGGER_H
