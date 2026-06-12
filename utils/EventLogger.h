#ifndef UTILS_EVENT_LOGGER_H
#define UTILS_EVENT_LOGGER_H

#include "core/base/ThreadSafeQueue.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

namespace utils {

enum class EventCategory {
    AUTH,
    SESSION,
    MESSAGE,
    VOICE,
    HUB,
    CHANNEL,
    USER,
    DB,
    COMMAND,
    SYSTEM,
    DEBUG
};

enum class LogDest : uint8_t { TERMINAL = 1, FILE = 2, BOTH = 3 };

struct LogEntry {
    std::string timestamp;
    EventCategory category;
    std::string user_id;
    std::string event;
    int64_t duration_ms;
    std::string details;
    LogDest dest;
};

/**
 * Async EventLogger - non-blocking log() calls with background writer thread
 */
class EventLogger {
   public:
    static EventLogger& instance() {
        static EventLogger logger;
        return logger;
    }

    void init(const std::filesystem::path& log_dir = "logs") {
        if (initialized_) return;

        try {
            if (!std::filesystem::exists(log_dir)) {
                std::filesystem::create_directories(log_dir);
            }
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "[ERROR] EventLogger: Failed to create log directory: " << e.what()
                      << "\n";
            return;
        }

        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::localtime(&t);

        std::ostringstream filename;
        filename << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S") << ".csv";

        log_path_ = log_dir / filename.str();
        file_.open(log_path_, std::ios::out | std::ios::app);

        if (file_.is_open()) {
            file_ << "timestamp,category,user_id,event,duration_ms,details\n";
            file_.flush();
            initialized_ = true;

            log_thread_ = std::jthread([this](std::stop_token stop) { run_writer(stop); });

            log(EventCategory::SYSTEM, "", "SERVER_START", 0, "Log file: " + log_path_.string());
        } else {
            std::cerr << "[ERROR] EventLogger: Failed to open log file: " << log_path_ << "\n";
        }
    }

    void shutdown() {
        if (!initialized_) return;

        log(EventCategory::SYSTEM, "", "SERVER_STOP", 0, "");

        queue_.stop();
        if (log_thread_.joinable()) {
            log_thread_.request_stop();
            log_thread_.join();
        }

        if (file_.is_open()) file_.close();
        initialized_ = false;
    }

    // Non-blocking - just enqueues
    void log(EventCategory category, std::string_view user_id, std::string_view event,
             int64_t duration_ms = 0, std::string_view details = "", LogDest dest = LogDest::FILE) {
        queue_.push(LogEntry{.timestamp = timestamp(),
                             .category = category,
                             .user_id = std::string(user_id),
                             .event = std::string(event),
                             .duration_ms = duration_ms,
                             .details = std::string(details),
                             .dest = dest});
    }

    // Convenience methods (all FILE only)
    void auth_failure(std::string_view user_id, std::string_view reason, int64_t ms) {
        log(EventCategory::AUTH, user_id, "AUTH_FAILURE", ms, reason);
    }
    void session_connect(std::string_view user_id, std::string_view ip = "") {
        log(EventCategory::SESSION, user_id, "CONNECT", 0, ip);
    }
    void session_disconnect(std::string_view user_id, std::string_view reason = "") {
        log(EventCategory::SESSION, user_id, "DISCONNECT", 0, reason);
    }
    void message_sent(std::string_view user_id, std::string_view ch, int64_t ms) {
        log(EventCategory::MESSAGE, user_id, "MSG_SENT", ms,
            std::string("channel:") + std::string(ch));
    }
    void voice_join(std::string_view user_id, std::string_view ch, std::string_view source = "") {
        std::string details = std::string("channel:") + std::string(ch);
        if (!source.empty()) details += " source:" + std::string(source);
        log(EventCategory::VOICE, user_id, "VOICE_JOIN", 0, details);
    }
    void voice_leave(std::string_view user_id, std::string_view ch) {
        log(EventCategory::VOICE, user_id, "VOICE_LEAVE", 0,
            std::string("channel:") + std::string(ch));
    }
    void db_query(std::string_view op, int64_t ms, std::string_view details = "") {
        log(EventCategory::DB, "", op, ms, details);
    }
    void debug(std::string_view component, std::string_view msg) {
        log(EventCategory::DEBUG, "", component, 0, msg, LogDest::TERMINAL);
    }

   private:
    EventLogger() = default;
    ~EventLogger() { shutdown(); }
    EventLogger(const EventLogger&) = delete;
    EventLogger& operator=(const EventLogger&) = delete;

    void run_writer(std::stop_token stop) {
        while (!stop.stop_requested()) {
            auto result = queue_.pop();
            if (!result.has_value()) break;
            write(result.value());
        }
        // Drain remaining
        while (auto result = queue_.try_pop()) {
            if (result.has_value())
                write(result.value());
            else
                break;
        }
    }

    void write(const LogEntry& e) {
        auto cat = cat_str(e.category);

        if (static_cast<int>(e.dest) & static_cast<int>(LogDest::TERMINAL)) {
            std::ostringstream oss;
            oss << "\033[90m[" << e.timestamp << "]\033[0m " << cat_color(e.category) << "[" << cat
                << "]\033[0m ";
            if (!e.user_id.empty()) oss << "\033[93m<" << e.user_id << ">\033[0m ";
            oss << e.event;
            if (e.duration_ms > 0) oss << " \033[90m(" << e.duration_ms << "ms)\033[0m";
            if (!e.details.empty()) oss << " " << e.details;
            std::cout << oss.str() << "\n";
        }

        if ((static_cast<int>(e.dest) & static_cast<int>(LogDest::FILE)) && file_.is_open()) {
            file_ << e.timestamp << "," << cat << "," << e.user_id << "," << e.event << ","
                  << e.duration_ms << "," << escape(e.details) << "\n";
            file_.flush();
        }
    }

    std::string timestamp() const {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::localtime(&t);
        auto ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "." << std::setfill('0') << std::setw(3)
            << ms.count();
        return oss.str();
    }

    static constexpr const char* cat_str(EventCategory c) {
        switch (c) {
            case EventCategory::AUTH:
                return "AUTH";
            case EventCategory::SESSION:
                return "SESSION";
            case EventCategory::MESSAGE:
                return "MESSAGE";
            case EventCategory::VOICE:
                return "VOICE";
            case EventCategory::HUB:
                return "HUB";
            case EventCategory::CHANNEL:
                return "CHANNEL";
            case EventCategory::USER:
                return "USER";
            case EventCategory::DB:
                return "DB";
            case EventCategory::COMMAND:
                return "COMMAND";
            case EventCategory::SYSTEM:
                return "SYSTEM";
            case EventCategory::DEBUG:
                return "DEBUG";
            default:
                return "UNKNOWN";
        }
    }

    static const char* cat_color(EventCategory c) {
        switch (c) {
            case EventCategory::AUTH:
                return "\033[36m";
            case EventCategory::SESSION:
                return "\033[35m";
            case EventCategory::MESSAGE:
                return "\033[34m";
            case EventCategory::VOICE:
                return "\033[33m";
            case EventCategory::HUB:
            case EventCategory::CHANNEL:
            case EventCategory::USER:
                return "\033[32m";
            case EventCategory::DB:
            case EventCategory::DEBUG:
                return "\033[90m";
            case EventCategory::COMMAND:
                return "\033[94m";
            case EventCategory::SYSTEM:
                return "\033[97m";
            default:
                return "\033[0m";
        }
    }

    static std::string escape(const std::string& s) {
        std::string r;
        bool q = false;
        for (char c : s) {
            if (c == '"') {
                r += "\"\"";
                q = true;
            } else if (c == ',' || c == '\n') {
                r += c;
                q = true;
            } else
                r += c;
        }
        return q ? "\"" + r + "\"" : r;
    }

    ThreadSafeQueue<LogEntry> queue_;
    std::jthread log_thread_;
    std::ofstream file_;
    std::filesystem::path log_path_;
    bool initialized_ = false;
};

#define LOG_EVENT(cat, user_id, event, duration, details) \
    ::utils::EventLogger::instance().log(cat, user_id, event, duration, details)

#define LOG_DEBUG(component, msg) ::utils::EventLogger::instance().debug(component, msg)

}  // namespace utils

#endif  // UTILS_EVENT_LOGGER_H
