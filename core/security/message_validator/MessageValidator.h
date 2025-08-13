#pragma once
#include "nlohmann/json.hpp"

#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

using json = nlohmann::json;

enum class MessageType {
    CHAT,
    JOIN_CHANNEL,
    LEAVE_CHANNEL,
    USER_LIST,
    PING,
    CALL_REQUEST,
    CALL_ACCEPT,
    CALL_REJECT,
    SYSTEM,
    UNKNOWN
};

struct MessageValidationResult {
    bool is_valid = false;
    std::string error_message;
    MessageType message_type = MessageType::UNKNOWN;
};

class MessageValidator {
   public:
    MessageValidator();
    ~MessageValidator();

    // Message validation
    MessageValidationResult validate_message(const json& message);
    bool is_message_format_valid(const json& message);
    bool is_message_size_valid(const json& message);
    bool is_message_content_safe(const json& message);

    // Content filtering
    bool contains_profanity(const std::string& text);
    bool contains_malicious_content(const std::string& text);
    bool is_sql_injection_attempt(const std::string& text);
    bool is_xss_attempt(const std::string& text);

    // Message type validation
    MessageType get_message_type(const json& message);
    bool validate_chat_message(const json& message);
    bool validate_join_message(const json& message);

    // Security validation
    bool is_channel_name_valid(const std::string& channel_name);
    bool is_username_valid(const std::string& username);
    bool is_message_id_unique(const std::string& message_id);

   private:
    std::unordered_set<std::string> profanity_words_;
    std::unordered_set<std::string> malicious_patterns_;
    std::unordered_set<std::string> recent_message_ids_;

    // Validation settings
    static constexpr int MAX_MESSAGE_SIZE = 4096;  // 4KB
    static constexpr int MAX_USERNAME_LENGTH = 32;
    static constexpr int MAX_CHANNEL_LENGTH = 64;
    static constexpr int MAX_TEXT_LENGTH = 2048;
    static constexpr int MESSAGE_ID_CACHE_SIZE = 10000;

    // Helper methods
    void load_profanity_filter();
    void load_security_patterns();
    std::string sanitize_string(const std::string& input);
    bool contains_pattern(const std::string& text, const std::unordered_set<std::string>& patterns);
};
