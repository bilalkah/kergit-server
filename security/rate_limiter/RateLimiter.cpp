#include "core/security/rate_limiter/RateLimiter.h"

#include <algorithm>
#include <chrono>

RateLimiter::RateLimiter() {}

RateLimiter::~RateLimiter() {}

bool RateLimiter::is_request_allowed(const std::string& client_id) {
    std::lock_guard<std::mutex> lock(rate_limit_mutex_);

    auto& limit_info = client_limits_[client_id];
    return check_rate_limit(limit_info, max_requests_per_minute_, RATE_LIMIT_WINDOW_SECONDS);
}

bool RateLimiter::is_message_allowed(const std::string& user_id) {
    std::lock_guard<std::mutex> lock(rate_limit_mutex_);

    auto& limit_info = client_limits_[user_id];
    return check_rate_limit(limit_info, max_messages_per_minute_, RATE_LIMIT_WINDOW_SECONDS);
}

bool RateLimiter::is_connection_allowed(const std::string& ip_address) {
    std::lock_guard<std::mutex> lock(rate_limit_mutex_);

    auto& limit_info = ip_limits_[ip_address];
    return check_rate_limit(limit_info, max_connections_per_minute_, RATE_LIMIT_WINDOW_SECONDS);
}

void RateLimiter::block_client(const std::string& client_id, int duration_seconds) {
    std::lock_guard<std::mutex> lock(rate_limit_mutex_);

    auto& limit_info = client_limits_[client_id];
    limit_info.is_blocked = true;

    // Ensure duration doesn't exceed maximum
    int safe_duration = std::min(duration_seconds, MAX_BLOCK_DURATION_SECONDS);
    limit_info.block_until = std::chrono::system_clock::now() + std::chrono::seconds(safe_duration);
}

void RateLimiter::unblock_client(const std::string& client_id) {
    std::lock_guard<std::mutex> lock(rate_limit_mutex_);

    auto it = client_limits_.find(client_id);
    if (it != client_limits_.end()) {
        it->second.is_blocked = false;
        it->second.block_until = std::chrono::system_clock::time_point{};
    }
}

bool RateLimiter::is_client_blocked(const std::string& client_id) {
    std::lock_guard<std::mutex> lock(rate_limit_mutex_);

    auto it = client_limits_.find(client_id);
    if (it == client_limits_.end()) {
        return false;
    }

    auto& limit_info = it->second;

    // Check if block has expired
    if (limit_info.is_blocked && std::chrono::system_clock::now() >= limit_info.block_until) {
        limit_info.is_blocked = false;
        limit_info.block_until = std::chrono::system_clock::time_point{};
    }

    return limit_info.is_blocked;
}

void RateLimiter::set_message_rate_limit(int messages_per_minute) {
    std::lock_guard<std::mutex> lock(rate_limit_mutex_);
    max_messages_per_minute_ = std::max(1, messages_per_minute);
}

void RateLimiter::set_connection_rate_limit(int connections_per_minute) {
    std::lock_guard<std::mutex> lock(rate_limit_mutex_);
    max_connections_per_minute_ = std::max(1, connections_per_minute);
}

void RateLimiter::set_request_rate_limit(int requests_per_minute) {
    std::lock_guard<std::mutex> lock(rate_limit_mutex_);
    max_requests_per_minute_ = std::max(1, requests_per_minute);
}

void RateLimiter::cleanup_expired_entries() {
    std::lock_guard<std::mutex> lock(rate_limit_mutex_);

    auto now = std::chrono::system_clock::now();
    auto cleanup_threshold = now - std::chrono::seconds(RATE_LIMIT_WINDOW_SECONDS * 2);

    // Cleanup client limits
    for (auto it = client_limits_.begin(); it != client_limits_.end();) {
        if (!it->second.is_blocked && it->second.last_request < cleanup_threshold) {
            it = client_limits_.erase(it);
        } else {
            ++it;
        }
    }

    // Cleanup IP limits
    for (auto it = ip_limits_.begin(); it != ip_limits_.end();) {
        if (!it->second.is_blocked && it->second.last_request < cleanup_threshold) {
            it = ip_limits_.erase(it);
        } else {
            ++it;
        }
    }
}

void RateLimiter::reset_rate_limits(const std::string& client_id) {
    std::lock_guard<std::mutex> lock(rate_limit_mutex_);

    auto it = client_limits_.find(client_id);
    if (it != client_limits_.end()) {
        // Reset request count and window, but preserve block status
        bool was_blocked = it->second.is_blocked;
        auto block_until = it->second.block_until;

        it->second = RateLimitInfo{};
        it->second.is_blocked = was_blocked;
        it->second.block_until = block_until;
    }
}

bool RateLimiter::check_rate_limit(RateLimitInfo& limit_info, int max_requests,
                                   int window_seconds) {
    auto now = std::chrono::system_clock::now();

    // Check if client is blocked
    if (limit_info.is_blocked) {
        if (now >= limit_info.block_until) {
            // Block has expired
            limit_info.is_blocked = false;
            limit_info.block_until = std::chrono::system_clock::time_point{};
        } else {
            return false;  // Still blocked
        }
    }

    // Check if we need to reset the window
    if (!is_in_current_window(limit_info.window_start, window_seconds)) {
        limit_info.request_count = 0;
        limit_info.window_start = now;
    }

    // Check if request is allowed
    if (limit_info.request_count >= max_requests) {
        return false;
    }

    // Update rate limit info
    update_rate_limit_info(limit_info);
    return true;
}

void RateLimiter::update_rate_limit_info(RateLimitInfo& limit_info) {
    auto now = std::chrono::system_clock::now();

    limit_info.request_count++;
    limit_info.last_request = now;

    // Initialize window start if not set
    if (limit_info.window_start == std::chrono::system_clock::time_point{}) {
        limit_info.window_start = now;
    }
}

bool RateLimiter::is_in_current_window(const std::chrono::system_clock::time_point& timestamp,
                                       int window_seconds) {
    if (timestamp == std::chrono::system_clock::time_point{}) {
        return false;
    }

    auto now = std::chrono::system_clock::now();
    auto window_duration = std::chrono::seconds(window_seconds);

    return (now - timestamp) < window_duration;
}
