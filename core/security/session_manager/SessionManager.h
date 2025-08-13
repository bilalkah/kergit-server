#pragma once
#include "core/security/authentication/Authentication.h"
#include "core/security/jwt_manager/JWTManager.h"

#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class SessionManager {
   public:
    SessionManager(std::shared_ptr<JWTManager> jwt_manager);
    ~SessionManager();

    // Session lifecycle
    std::string create_session(const std::string& user_id, const std::string& client_ip,
                               const std::string& user_agent);
    bool validate_session(const std::string& session_id);
    bool refresh_session(const std::string& session_id);
    bool destroy_session(const std::string& session_id);

    // Session management
    std::shared_ptr<SessionInfo> get_session_info(const std::string& session_id);
    bool update_last_activity(const std::string& session_id);
    void cleanup_expired_sessions();

    // Security features
    int get_active_sessions_count(const std::string& user_id);
    bool is_session_from_trusted_ip(const std::string& session_id, const std::string& client_ip);
    void force_logout_user(const std::string& user_id);

    // Session limits
    static constexpr int MAX_SESSIONS_PER_USER = 5;
    static constexpr int SESSION_TIMEOUT_MINUTES = 30;

   private:
    std::shared_ptr<JWTManager> jwt_manager_;
    std::unordered_map<std::string, std::shared_ptr<SessionInfo>> sessions_;
    std::unordered_map<std::string, std::vector<std::string>> user_sessions_;

    // Helper methods
    std::string generate_session_id();
    bool is_session_expired(const std::shared_ptr<SessionInfo>& session);
    void remove_session_internal(const std::string& session_id);
};
