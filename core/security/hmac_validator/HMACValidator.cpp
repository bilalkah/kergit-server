#include "core/security/hmac_validator/HMACValidator.h"

#include <chrono>
#include <iomanip>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <sstream>

HMACValidator::HMACValidator(const std::string& secret_key) : secret_key_(secret_key) {}

HMACValidator::~HMACValidator() { cleanup_old_messages(); }

std::string HMACValidator::generate_hmac(const std::string& message) {
    return hmac_sha256(message, secret_key_);
}

std::string HMACValidator::generate_hmac_with_timestamp(const std::string& message) {
    std::string timestamp = get_current_timestamp();
    std::string data = message + "|" + timestamp;
    std::string hmac = hmac_sha256(data, secret_key_);
    return hmac + "|" + timestamp;
}

bool HMACValidator::verify_hmac(const std::string& message, const std::string& hmac) {
    std::string expected_hmac = generate_hmac(message);

    // Constant time comparison to prevent timing attacks
    if (expected_hmac.length() != hmac.length()) {
        return false;
    }

    int result = 0;
    for (size_t i = 0; i < expected_hmac.length(); ++i) {
        result |= expected_hmac[i] ^ hmac[i];
    }

    return result == 0;
}

bool HMACValidator::verify_hmac_with_timestamp(const std::string& message,
                                               const std::string& hmac_with_timestamp,
                                               int max_age_seconds) {
    // Parse HMAC and timestamp
    size_t delimiter_pos = hmac_with_timestamp.rfind('|');
    if (delimiter_pos == std::string::npos) {
        return false;
    }

    std::string hmac = hmac_with_timestamp.substr(0, delimiter_pos);
    std::string timestamp = hmac_with_timestamp.substr(delimiter_pos + 1);

    // Validate timestamp
    if (!is_timestamp_valid(timestamp, max_age_seconds)) {
        return false;
    }

    // Verify HMAC
    std::string data = message + "|" + timestamp;
    return verify_hmac(data, hmac);
}

std::string HMACValidator::sign_message(const std::string& message) {
    return generate_hmac_with_timestamp(message);
}

bool HMACValidator::verify_message_signature(const std::string& message,
                                             const std::string& signature) {
    return verify_hmac_with_timestamp(message, signature);
}

void HMACValidator::rotate_key(const std::string& new_key) {
    secret_key_ = new_key;
    // Clear seen messages as they were signed with the old key
    seen_messages_.clear();
}

bool HMACValidator::is_replay_attack(const std::string& message_id, const std::string& timestamp) {
    auto now = std::chrono::system_clock::now();

    // Check if we've seen this message before
    auto it = seen_messages_.find(message_id);
    if (it != seen_messages_.end()) {
        return true;  // Replay attack detected
    }

    // Validate timestamp is within acceptable window
    if (!is_timestamp_valid(timestamp, MESSAGE_REPLAY_WINDOW_SECONDS)) {
        return true;  // Timestamp too old or invalid
    }

    // Record this message
    seen_messages_[message_id] = now;

    // Cleanup old messages periodically
    static auto last_cleanup = now;
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_cleanup).count() >=
        CLEANUP_INTERVAL_SECONDS) {
        cleanup_old_messages();
        last_cleanup = now;
    }

    return false;
}

std::string HMACValidator::hmac_sha256(const std::string& data, const std::string& key) {
    unsigned char result[SHA256_DIGEST_LENGTH];
    unsigned int result_len;

    HMAC(EVP_sha256(), key.c_str(), static_cast<int>(key.length()),
         reinterpret_cast<const unsigned char*>(data.c_str()), data.length(), result, &result_len);

    // Convert to hex string
    std::stringstream ss;
    for (unsigned int i = 0; i < result_len; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(result[i]);
    }

    return ss.str();
}

std::string HMACValidator::get_current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto timestamp =
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    return std::to_string(timestamp);
}

bool HMACValidator::is_timestamp_valid(const std::string& timestamp, int max_age_seconds) {
    try {
        long long timestamp_value = std::stoll(timestamp);
        auto now = std::chrono::system_clock::now();
        auto current_timestamp =
            std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

        long long age = current_timestamp - timestamp_value;

        // Check if timestamp is not from the future (with small tolerance)
        if (age < -60) {  // Allow 1 minute clock skew
            return false;
        }

        // Check if timestamp is not too old
        return age <= max_age_seconds;
    } catch (const std::exception&) {
        return false;  // Invalid timestamp format
    }
}

void HMACValidator::cleanup_old_messages() {
    auto now = std::chrono::system_clock::now();
    auto cutoff_time = now - std::chrono::seconds(MESSAGE_REPLAY_WINDOW_SECONDS);

    for (auto it = seen_messages_.begin(); it != seen_messages_.end();) {
        if (it->second < cutoff_time) {
            it = seen_messages_.erase(it);
        } else {
            ++it;
        }
    }
}
