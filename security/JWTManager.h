#pragma once
#include <chrono>
#include <string>
#include <unordered_map>

struct JWTClaims {
    std::string user_id;
    std::string username;
    std::string role;
    std::chrono::system_clock::time_point issued_at;
    std::chrono::system_clock::time_point expires_at;
    std::string session_id;
};

class JWTManager {
   public:
    JWTManager(const std::string& secret_key);
    ~JWTManager();

    // Token operations
    std::string generate_token(const JWTClaims& claims);
    bool verify_token(const std::string& token);
    JWTClaims decode_token(const std::string& token);

    // Token validation
    bool is_token_expired(const std::string& token);
    bool is_token_valid(const std::string& token);

    // Security
    void rotate_secret();
    bool revoke_token(const std::string& token);

   private:
    std::string secret_key_;
    std::unordered_map<std::string, bool> revoked_tokens_;

    // JWT settings
    static constexpr int TOKEN_EXPIRY_HOURS = 24;
    static constexpr int REFRESH_TOKEN_EXPIRY_DAYS = 30;

    // Helper methods
    std::string base64_encode(const std::string& input);
    std::string base64_decode(const std::string& input);
    std::string hmac_sha256(const std::string& data, const std::string& key);
};
