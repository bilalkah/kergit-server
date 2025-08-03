#include "User.h"

#include <algorithm>
#include <chrono>

bool User::is_account_locked() const {
    if (security_metrics.account_locked_until > std::chrono::system_clock::now()) {
        return true;
    }
    return false;
}

bool User::is_session_valid(const std::string& session_id) const {
    auto it = std::find_if(active_sessions.begin(), active_sessions.end(),
                           [&session_id](const SecureSession& session) {
                               return session.session_id == session_id && session.is_active;
                           });

    if (it != active_sessions.end()) {
        // Check if session hasn't expired (24 hours)
        auto now = std::chrono::system_clock::now();
        auto session_age = std::chrono::duration_cast<std::chrono::hours>(now - it->created_at);
        return session_age.count() < 24;
    }
    return false;
}

bool User::has_valid_session() const {
    return std::any_of(
        active_sessions.begin(), active_sessions.end(),
        [this](const SecureSession& session) { return is_session_valid(session.session_id); });
}

void User::add_session(const SecureSession& session) {
    // Remove old sessions if we have too many
    while (active_sessions.size() >= MAX_SESSIONS_PER_USER) {
        active_sessions.erase(active_sessions.begin());
    }
    active_sessions.push_back(session);
}

void User::remove_session(const std::string& session_id) {
    active_sessions.erase(std::remove_if(active_sessions.begin(), active_sessions.end(),
                                         [&session_id](const SecureSession& session) {
                                             return session.session_id == session_id;
                                         }),
                          active_sessions.end());
}

void User::lock_account(int duration_minutes) {
    security_metrics.account_locked_until =
        std::chrono::system_clock::now() + std::chrono::minutes(duration_minutes);

    // Clear all active sessions when account is locked
    active_sessions.clear();
}

void User::unlock_account() {
    security_metrics.account_locked_until = std::chrono::system_clock::time_point{};
    security_metrics.failed_login_attempts = 0;
}

bool User::is_ip_trusted(const std::string& ip) const {
    return std::find(security_metrics.trusted_ips.begin(), security_metrics.trusted_ips.end(),
                     ip) != security_metrics.trusted_ips.end();
}

void User::add_trusted_ip(const std::string& ip) {
    if (!is_ip_trusted(ip)) {
        security_metrics.trusted_ips.push_back(ip);
    }
}

void User::update_last_activity() { last_activity = std::chrono::system_clock::now(); }

bool User::is_recently_active(int minutes) const {
    auto now = std::chrono::system_clock::now();
    auto time_diff = std::chrono::duration_cast<std::chrono::minutes>(now - last_activity);
    return time_diff.count() <= minutes;
}
