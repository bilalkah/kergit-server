#pragma once
#include <chrono>
#include <ctime>
#include <memory>
#include <string>
#include <unordered_map>

struct AuthUser {
    std::string id;
    std::string username;
    std::string email;
    std::string password_hash;
    std::string salt;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_login;
    bool is_active = true;
    std::string role = "user";
};

struct SessionInfo {
    std::string session_id;
    std::string user_id;
    std::string jwt_token;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point expires_at;
    std::chrono::system_clock::time_point last_activity;
    std::string client_ip;
    std::string user_agent;
    bool is_valid = true;
};

class Authentication {
   public:
    Authentication();
    ~Authentication();

    // User registration and authentication
    bool register_user(const std::string& username, const std::string& email,
                       const std::string& password);
    bool authenticate_user(const std::string& username, const std::string& password);

    // Password security
    std::string hash_password(const std::string& password, const std::string& salt = "");
    std::string generate_salt();
    bool verify_password(const std::string& password, const std::string& hash,
                         const std::string& salt);

    // User management
    std::shared_ptr<AuthUser> get_user(const std::string& user_id);
    std::shared_ptr<AuthUser> get_user_by_username(const std::string& username);
    bool update_last_login(const std::string& user_id);

    // Session validation
    bool is_session_valid(const std::string& session_id);
    std::shared_ptr<SessionInfo> get_session(const std::string& session_id);

   private:
    std::unordered_map<std::string, std::shared_ptr<AuthUser>> users_;
    std::unordered_map<std::string, std::shared_ptr<AuthUser>> username_to_user_;

    // Security settings
    static constexpr int MIN_PASSWORD_LENGTH = 6;  // Reduced for demo
    static constexpr int SALT_LENGTH = 16;         // Reduced for demo
};
