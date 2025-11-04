#include "infra/security/validation/MessageValidator.h"

#include <algorithm>
#include <regex>

namespace infra::security::validation {

MessageValidator::MessageValidator(const app::Dispatcher& dispatcher) {
    // Load command operations from dispatcher
    registered_commands_ = std::move(dispatcher.registered_commands());
    load_profanity_filter();
    load_security_patterns();
}

MessageValidator::~MessageValidator() = default;

MessageValidationResult MessageValidator::validate_message(const std::string_view& raw_message) {
    MessageValidationResult result;

    auto message_opt = is_message_format_valid(raw_message);
    if (!message_opt.has_value()) {
        result.error_message = "Invalid message format.";
        return result;
    }

    json message = message_opt.value();

    if (!is_message_size_valid(message)) {
        result.is_valid = false;
        result.error_message = "Message size exceeds limit.";
        return result;
    }

    if (!is_message_content_safe(message)) {
        result.is_valid = false;
        result.error_message = "Message contains unsafe content.";
        return result;
    }

    result.message_type = get_message_type(message);

    // Type-specific validation
    switch (result.message_type) {
        case MessageType::CMD:
            if (!validate_cmd_message(message)) {
                result.error_message = "Invalid request operation";
                return result;
            }
            break;
        default:
            // Other message types pass basic validation
            break;
    }

    result.is_valid = true;
    result.message = message;

    return result;
}

std::optional<json> MessageValidator::is_message_format_valid(const std::string_view& raw_message) {
    json message;
    try {
        message = json::parse(raw_message);
    } catch (...) {
        return std::nullopt;
    }

    // Check if it's a valid JSON object
    if (!message.is_object()) {
        return std::nullopt;
    }

    // Check for required fields
    if (!message.contains("type") || !message["type"].is_string()) {
        return std::nullopt;
    }

    if (!message.contains("ts") || !message["ts"].is_number_integer()) {
        return std::nullopt;
    }

    return message;
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

bool MessageValidator::validate_cmd_message(const json& message) {
    if (!message.contains("type") || !message["type"].is_string()) {
        return false;
    }
    std::string operation = message["type"];
    return registered_commands_.find(operation) != registered_commands_.end();
}

void MessageValidator::load_profanity_filter() {
    // Load common profanity words (sample set)
    profanity_words_ = {
        "spam", "scam", "fraud", "phishing"
        // Add more words as needed
    };
}

MessageType MessageValidator::get_message_type(const json& message) {
    if (!message.contains("type") || !message["type"].is_string()) {
        return MessageType::UNKNOWN;
    }

    std::string type = message["type"];

    if (type == "type") return MessageType::CMD;
    if (type == "system") return MessageType::SYSTEM;

    return MessageType::UNKNOWN;
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

}  // namespace infra::security::validation
