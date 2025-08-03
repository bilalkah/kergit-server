#pragma once
#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>

struct RateLimitInfo {
    int request_count = 0;
    std::chrono::system_clock::time_point window_start;
    std::chrono::system_clock::time_point last_request;
    bool is_blocked = false;
    std::chrono::system_clock::time_point block_until;
};

class RateLimiter {
   public:
    RateLimiter();
    ~RateLimiter();

    // Rate limiting
    bool is_request_allowed(const std::string& client_id);
    bool is_message_allowed(const std::string& user_id);
    bool is_connection_allowed(const std::string& ip_address);

    // Block management
    void block_client(const std::string& client_id, int duration_seconds);
    void unblock_client(const std::string& client_id);
    bool is_client_blocked(const std::string& client_id);

    // Rate limit configuration
    void set_message_rate_limit(int messages_per_minute);
    void set_connection_rate_limit(int connections_per_minute);
    void set_request_rate_limit(int requests_per_minute);

    // Cleanup and maintenance
    void cleanup_expired_entries();
    void reset_rate_limits(const std::string& client_id);

   private:
    std::unordered_map<std::string, RateLimitInfo> client_limits_;
    std::unordered_map<std::string, RateLimitInfo> ip_limits_;
    std::mutex rate_limit_mutex_;

    // Rate limit settings (per minute)
    int max_messages_per_minute_ = 60;     // 1 message per second average
    int max_connections_per_minute_ = 10;  // Connection attempts
    int max_requests_per_minute_ = 120;    // General requests

    // Block settings
    static constexpr int DEFAULT_BLOCK_DURATION_SECONDS = 300;  // 5 minutes
    static constexpr int MAX_BLOCK_DURATION_SECONDS = 3600;     // 1 hour
    static constexpr int RATE_LIMIT_WINDOW_SECONDS = 60;        // 1 minute

    // Helper methods
    bool check_rate_limit(RateLimitInfo& limit_info, int max_requests, int window_seconds);
    void update_rate_limit_info(RateLimitInfo& limit_info);
    bool is_in_current_window(const std::chrono::system_clock::time_point& timestamp,
                              int window_seconds);
};
