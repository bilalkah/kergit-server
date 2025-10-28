#pragma once
#include <chrono>
#include <string>
#include <vector>

enum class Authority { USER, MODERATOR, ADMIN };

enum class UserStatus { OFFLINE, ONLINE, AWAY, BUSY, DO_NOT_DISTURB };

struct SecureSession {
    std::string session_id;
    std::string jwt_token;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_activity;
    std::string client_ip;
    std::string user_agent;
    bool is_active = true;
};

struct SecurityMetrics {
    int failed_login_attempts = 0;
    std::chrono::system_clock::time_point last_failed_login;
    std::chrono::system_clock::time_point account_locked_until;
    std::vector<std::string> trusted_ips;
    bool requires_2fa = false;
    std::string recovery_email;
};

class User {
   public:
    // Basic user information
    std::string id;
    std::string username;
    std::string full_name;
    std::string email;
    UserStatus status = UserStatus::ONLINE;
    std::string current_channel;

    // Security information
    std::string password_hash;
    std::string salt;
    SecurityMetrics security_metrics;
    std::vector<SecureSession> active_sessions;

    // Timestamps
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_login;
    std::chrono::system_clock::time_point last_activity;

    // User preferences and settings
    bool notifications_enabled = true;
    bool sound_enabled = true;
    std::string preferred_language = "en";

    // Security methods
    bool is_account_locked() const;
    bool is_session_valid(const std::string& session_id) const;
    bool has_valid_session() const;
    void add_session(const SecureSession& session);
    void remove_session(const std::string& session_id);
    void lock_account(int duration_minutes = 30);
    void unlock_account();
    bool is_ip_trusted(const std::string& ip) const;
    void add_trusted_ip(const std::string& ip);

    // Activity tracking
    void update_last_activity();
    bool is_recently_active(int minutes = 5) const;

   private:
    static constexpr int MAX_SESSIONS_PER_USER = 1;
    static constexpr int MAX_FAILED_ATTEMPTS = 5;
    static constexpr int ACCOUNT_LOCK_DURATION_MINUTES = 30;
};