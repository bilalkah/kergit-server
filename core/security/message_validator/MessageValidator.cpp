#include "core/security/message_validator/MessageValidator.h"

#include <algorithm>
#include <regex>

MessageValidator::MessageValidator() {
    load_profanity_filter();
    load_security_patterns();
}

MessageValidator::~MessageValidator() {}

MessageValidationResult MessageValidator::validate_message(const json& message) {
    MessageValidationResult result;

    // Check basic format
    if (!is_message_format_valid(message)) {
        result.error_message = "Invalid message format";
        return result;
    }

    // Check message size
    if (!is_message_size_valid(message)) {
        result.error_message = "Message too large";
        return result;
    }

    // Check content safety
    if (!is_message_content_safe(message)) {
        result.error_message = "Message contains unsafe content";
        return result;
    }

    // Determine message type
    result.message_type = get_message_type(message);

    // Type-specific validation
    switch (result.message_type) {
        case MessageType::CHAT:
            if (!validate_chat_message(message)) {
                result.error_message = "Invalid chat message";
                return result;
            }
            break;
        case MessageType::JOIN_CHANNEL:
        case MessageType::LEAVE_CHANNEL:
            if (!validate_join_message(message)) {
                result.error_message = "Invalid channel operation";
                return result;
            }
            break;
        default:
            // Other message types pass basic validation
            break;
    }

    result.is_valid = true;
    return result;
}

bool MessageValidator::is_message_format_valid(const json& message) {
    // Check if it's a valid JSON object
    if (!message.is_object()) {
        return false;
    }

    // Check for required fields
    if (!message.contains("type") || !message["type"].is_string()) {
        return false;
    }

    if (!message.contains("id") || !message["id"].is_string()) {
        return false;
    }

    if (!message.contains("timestamp")) {
        return false;
    }

    return true;
}

bool MessageValidator::is_message_size_valid(const json& message) {
    std::string serialized = message.dump();
    return serialized.length() <= MAX_MESSAGE_SIZE;
}

bool MessageValidator::is_message_content_safe(const json& message) {
    // Check all string fields for malicious content
    for (auto& [key, value] : message.items()) {
        if (value.is_string()) {
            std::string str_value = value.get<std::string>();

            if (contains_profanity(str_value) || contains_malicious_content(str_value) ||
                is_sql_injection_attempt(str_value) || is_xss_attempt(str_value)) {
                return false;
            }
        }
    }

    return true;
}

bool MessageValidator::contains_profanity(const std::string& text) {
    std::string lower_text = text;
    std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(), ::tolower);

    return contains_pattern(lower_text, profanity_words_);
}

bool MessageValidator::contains_malicious_content(const std::string& text) {
    return contains_pattern(text, malicious_patterns_);
}

bool MessageValidator::is_sql_injection_attempt(const std::string& text) {
    std::string lower_text = text;
    std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(), ::tolower);

    // Common SQL injection patterns
    std::vector<std::string> sql_patterns = {
        "union select", "drop table", "delete from", "insert into", "update set",
        "exec ",        "execute",    "sp_",         "xp_",         "\\\\",
        "--",           "/*",         "*/"};

    for (const auto& pattern : sql_patterns) {
        if (lower_text.find(pattern) != std::string::npos) {
            return true;
        }
    }

    return false;
}

bool MessageValidator::is_xss_attempt(const std::string& text) {
    std::string lower_text = text;
    std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(), ::tolower);

    // Common XSS patterns
    std::vector<std::string> xss_patterns = {
        "<script",  "</script>",    "javascript:", "onload=", "onerror=",
        "onclick=", "onmouseover=", "eval(",       "alert(",  "document.cookie"};

    for (const auto& pattern : xss_patterns) {
        if (lower_text.find(pattern) != std::string::npos) {
            return true;
        }
    }

    return false;
}

MessageType MessageValidator::get_message_type(const json& message) {
    if (!message.contains("type") || !message["type"].is_string()) {
        return MessageType::UNKNOWN;
    }

    std::string type = message["type"];

    if (type == "chat") return MessageType::CHAT;
    if (type == "join") return MessageType::JOIN_CHANNEL;
    if (type == "leave") return MessageType::LEAVE_CHANNEL;
    if (type == "users") return MessageType::USER_LIST;
    if (type == "ping") return MessageType::PING;
    if (type == "call_request") return MessageType::CALL_REQUEST;
    if (type == "call_accept") return MessageType::CALL_ACCEPT;
    if (type == "call_reject") return MessageType::CALL_REJECT;
    if (type == "system") return MessageType::SYSTEM;

    return MessageType::UNKNOWN;
}

bool MessageValidator::validate_chat_message(const json& message) {
    // Check for required chat message fields
    if (!message.contains("text") || !message["text"].is_string()) {
        return false;
    }

    if (!message.contains("username") || !message["username"].is_string()) {
        return false;
    }

    if (!message.contains("channel") || !message["channel"].is_string()) {
        return false;
    }

    // Validate field contents
    std::string text = message["text"];
    std::string username = message["username"];
    std::string channel = message["channel"];

    if (text.length() > MAX_TEXT_LENGTH) {
        return false;
    }

    if (!is_username_valid(username)) {
        return false;
    }

    if (!is_channel_name_valid(channel)) {
        return false;
    }

    // Check message ID uniqueness
    if (message.contains("id")) {
        std::string message_id = message["id"];
        if (!is_message_id_unique(message_id)) {
            return false;
        }
    }

    return true;
}

bool MessageValidator::validate_join_message(const json& message) {
    // Check for required join/leave message fields
    if (!message.contains("username") || !message["username"].is_string()) {
        return false;
    }

    if (!message.contains("channel") || !message["channel"].is_string()) {
        return false;
    }

    std::string username = message["username"];
    std::string channel = message["channel"];

    return is_username_valid(username) && is_channel_name_valid(channel);
}

bool MessageValidator::is_channel_name_valid(const std::string& channel_name) {
    if (channel_name.empty() || channel_name.length() > MAX_CHANNEL_LENGTH) {
        return false;
    }

    // Channel names should only contain alphanumeric, underscore, and dash
    std::regex channel_regex("^[a-zA-Z0-9_-]+$");
    return std::regex_match(channel_name, channel_regex);
}

bool MessageValidator::is_username_valid(const std::string& username) {
    if (username.empty() || username.length() > MAX_USERNAME_LENGTH) {
        return false;
    }

    // Usernames should only contain alphanumeric and underscore
    std::regex username_regex("^[a-zA-Z0-9_]+$");
    return std::regex_match(username, username_regex);
}

bool MessageValidator::is_message_id_unique(const std::string& message_id) {
    if (recent_message_ids_.count(message_id) > 0) {
        return false;  // Duplicate message ID
    }

    // Add to cache
    recent_message_ids_.insert(message_id);

    // Maintain cache size
    if (recent_message_ids_.size() > MESSAGE_ID_CACHE_SIZE) {
        // Remove some old entries (simple approach - clear half)
        auto it = recent_message_ids_.begin();
        std::advance(it, MESSAGE_ID_CACHE_SIZE / 2);
        recent_message_ids_.erase(recent_message_ids_.begin(), it);
    }

    return true;
}

void MessageValidator::load_profanity_filter() {
    // Load common profanity words (sample set)
    profanity_words_ = {
        "spam", "scam", "fraud", "phishing"
        // Add more words as needed
    };
}

void MessageValidator::load_security_patterns() {
    // Load malicious patterns
    malicious_patterns_ = {
        "eval(",    "exec(",    "system(",       "shell_exec", "file_get_contents",
        "include(", "require(", "base64_decode", "gzinflate",  "str_rot13"};
}

std::string MessageValidator::sanitize_string(const std::string& input) {
    std::string sanitized = input;

    // Remove or escape dangerous characters
    std::replace(sanitized.begin(), sanitized.end(), '<', ' ');
    std::replace(sanitized.begin(), sanitized.end(), '>', ' ');
    std::replace(sanitized.begin(), sanitized.end(), '&', ' ');
    std::replace(sanitized.begin(), sanitized.end(), '"', ' ');
    std::replace(sanitized.begin(), sanitized.end(), '\'', ' ');

    return sanitized;
}

bool MessageValidator::contains_pattern(const std::string& text,
                                        const std::unordered_set<std::string>& patterns) {
    for (const auto& pattern : patterns) {
        if (text.find(pattern) != std::string::npos) {
            return true;
        }
    }
    return false;
}
