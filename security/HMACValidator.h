#pragma once
#include <chrono>
#include <string>
#include <unordered_map>

class HMACValidator {
   public:
    HMACValidator(const std::string& secret_key);
    ~HMACValidator();

    // HMAC operations
    std::string generate_hmac(const std::string& message);
    std::string generate_hmac_with_timestamp(const std::string& message);
    bool verify_hmac(const std::string& message, const std::string& hmac);
    bool verify_hmac_with_timestamp(const std::string& message, const std::string& hmac,
                                    int max_age_seconds = 300);

    // Message integrity
    std::string sign_message(const std::string& message);
    bool verify_message_signature(const std::string& message, const std::string& signature);

    // Security features
    void rotate_key(const std::string& new_key);
    bool is_replay_attack(const std::string& message_id, const std::string& timestamp);

   private:
    std::string secret_key_;
    std::unordered_map<std::string, std::chrono::system_clock::time_point> seen_messages_;

    // HMAC settings
    static constexpr int MESSAGE_REPLAY_WINDOW_SECONDS = 300;  // 5 minutes
    static constexpr int CLEANUP_INTERVAL_SECONDS = 600;       // 10 minutes

    // Helper methods
    std::string hmac_sha256(const std::string& data, const std::string& key);
    std::string get_current_timestamp();
    bool is_timestamp_valid(const std::string& timestamp, int max_age_seconds);
    void cleanup_old_messages();
};
