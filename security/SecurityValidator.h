#pragma once
#include "Authentication.h"
#include "HMACValidator.h"
#include "MessageValidator.h"
#include "RateLimiter.h"

#include <memory>
#include <string>
#include <vector>

struct SecurityValidationResult {
    bool is_valid = false;
    std::string error_message;
    std::string error_code;
    bool should_block_client = false;
    bool should_log_incident = false;
};

enum class SecurityThreatLevel { LOW, MEDIUM, HIGH, CRITICAL };

struct SecurityIncident {
    std::string incident_id;
    std::string client_id;
    std::string ip_address;
    SecurityThreatLevel threat_level;
    std::string description;
    std::chrono::system_clock::time_point timestamp;
    std::string user_agent;
    json incident_data;
};

class SecurityValidator {
   public:
    SecurityValidator(std::shared_ptr<Authentication> auth,
                      std::shared_ptr<MessageValidator> msg_validator,
                      std::shared_ptr<RateLimiter> rate_limiter,
                      std::shared_ptr<HMACValidator> hmac_validator);
    ~SecurityValidator();

    // Comprehensive security validation
    SecurityValidationResult validate_connection_request(const std::string& origin,
                                                         const std::string& user_agent,
                                                         const std::string& ip_address);

    SecurityValidationResult validate_authentication_request(const json& auth_message,
                                                             const std::string& client_ip);

    SecurityValidationResult validate_message_request(const json& message,
                                                      const std::string& session_id,
                                                      const std::string& client_ip);

    // Security checks
    bool is_origin_allowed(const std::string& origin);
    bool is_user_agent_suspicious(const std::string& user_agent);
    bool is_ip_address_blocked(const std::string& ip_address);
    bool is_request_frequency_suspicious(const std::string& client_id);

    // Threat detection
    SecurityThreatLevel assess_threat_level(const json& message, const std::string& client_id);
    bool is_potential_attack(const json& message);
    bool is_brute_force_attempt(const std::string& client_ip);

    // Security incident management
    void log_security_incident(const SecurityIncident& incident);
    std::vector<SecurityIncident> get_recent_incidents(int hours = 24);
    void block_suspicious_client(const std::string& client_id, const std::string& reason);

    // Configuration
    void add_allowed_origin(const std::string& origin);
    void remove_allowed_origin(const std::string& origin);
    void set_security_level(SecurityThreatLevel level);

   private:
    std::shared_ptr<Authentication> auth_;
    std::shared_ptr<MessageValidator> msg_validator_;
    std::shared_ptr<RateLimiter> rate_limiter_;
    std::shared_ptr<HMACValidator> hmac_validator_;

    std::vector<std::string> allowed_origins_;
    std::vector<std::string> blocked_ips_;
    std::vector<SecurityIncident> security_incidents_;
    SecurityThreatLevel current_security_level_ = SecurityThreatLevel::MEDIUM;

    // Security thresholds
    static constexpr int MAX_FAILED_AUTH_ATTEMPTS = 5;
    static constexpr int BRUTE_FORCE_THRESHOLD = 10;
    static constexpr int INCIDENT_LOG_MAX_SIZE = 10000;

    // Helper methods
    std::string generate_incident_id();
    bool is_automated_client(const std::string& user_agent);
    bool contains_suspicious_patterns(const json& message);
    void cleanup_old_incidents();
};
