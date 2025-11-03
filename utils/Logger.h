#ifndef UTILS_LOGGER_H
#define UTILS_LOGGER_H

#include <ctime>
#include <iomanip>
#include <iostream>
#include <string>

namespace utils {
enum class LogLevel { INFO, WARN, ERROR };

namespace Color {
inline constexpr const char* RESET = "\033[0m";
inline constexpr const char* RED = "\033[31m";
inline constexpr const char* GREEN = "\033[32m";
inline constexpr const char* YELLOW = "\033[33m";
}  // namespace Color

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
        case LogLevel::WARN:
            return "WARN";
        case LogLevel::ERROR:
            return "ERROR";
        default:
            return "LOG";
    }
}

inline const char* level_to_color(LogLevel lvl) {
    switch (lvl) {
        case LogLevel::INFO:
            return Color::GREEN;
        case LogLevel::WARN:
            return Color::YELLOW;
        case LogLevel::ERROR:
            return Color::RED;
        default:
            return Color::RESET;
    }
}

inline void log_line(LogLevel lvl, const std::string& msg) {
    const char* color = level_to_color(lvl);
    std::cout << color << "[" << timestamp() << "] " << "[" << level_to_string(lvl) << "] " << msg
              << Color::RESET << std::endl;
}

};  // namespace utils

#endif  // UTILS_LOGGER_H
