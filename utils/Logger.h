#ifndef UTILS_LOGGER_H
#define UTILS_LOGGER_H

#include <atomic>
#include <ctime>
#include <fmt/format.h>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>

namespace utils {

/**
 * Log levels for terminal output
 */
enum class LogLevel {
    DEBUG,  // Gray - verbose debug info
    INFO,   // Green - general information
    WARN,   // Yellow - warnings
    ERROR   // Red - errors
};

namespace Color {
inline constexpr const char* RESET = "\033[0m";
inline constexpr const char* GRAY = "\033[90m";
inline constexpr const char* RED = "\033[31m";
inline constexpr const char* GREEN = "\033[32m";
inline constexpr const char* YELLOW = "\033[33m";
inline constexpr const char* BLUE = "\033[34m";
inline constexpr const char* MAGENTA = "\033[35m";
inline constexpr const char* CYAN = "\033[36m";
inline constexpr const char* WHITE = "\033[97m";
}  // namespace Color

namespace detail {
// Single mutex serializing every write to the diagnostic stdout sink. The backend logs from many
// threads (workers, loops, the EventLogger writer thread); without this, concurrent `std::cout <<`
// interleaves and lines tear. All terminal diagnostics MUST go through write_stdout_line() so they
// share this one lock.
inline std::mutex& stdout_mutex() {
    static std::mutex m;
    return m;
}

inline std::atomic<LogLevel>& min_level() {
    static std::atomic<LogLevel> level{LogLevel::DEBUG};
    return level;
}
}  // namespace detail

// Runtime minimum level for log_line(); messages below it are dropped. Defaults to DEBUG (log all).
inline void set_log_level(LogLevel level) {
    detail::min_level().store(level, std::memory_order_relaxed);
}
inline LogLevel get_log_level() { return detail::min_level().load(std::memory_order_relaxed); }

// The single synchronized stdout sink. Writes exactly one line (a trailing newline is appended)
// under the shared lock. This is the one place the process writes diagnostics to the terminal.
inline void write_stdout_line(std::string_view line) {
    std::lock_guard<std::mutex> lock(detail::stdout_mutex());
    std::cout << line << '\n';
}

inline std::string timestamp() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_r(&t, &tm);  // thread-safe; std::localtime shares a static tm (data race)
    char buf[16];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", &tm);
    return std::string(buf);
}

inline const char* level_to_string(LogLevel lvl) {
    switch (lvl) {
        case LogLevel::DEBUG:
            return "DEBUG";
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
        case LogLevel::DEBUG:
            return Color::GRAY;
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

/**
 * Print a diagnostic log line to the terminal. Thread-safe and level-filtered; the actual write
 * goes through the shared write_stdout_line() sink.
 */
inline void log_line(LogLevel lvl, const std::string& msg) {
    if (static_cast<int>(lvl) < static_cast<int>(get_log_level())) {
        return;
    }
    write_stdout_line(fmt::format("{}[{}] [{}] {}{}", level_to_color(lvl), timestamp(),
                                  level_to_string(lvl), msg, Color::RESET));
}

}  // namespace utils

#endif  // UTILS_LOGGER_H
