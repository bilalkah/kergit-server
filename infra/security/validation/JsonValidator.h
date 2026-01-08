#ifndef INFRA_SECURITY_VALIDATION_MESSAGEVALIDATOR_H
#define INFRA_SECURITY_VALIDATION_MESSAGEVALIDATOR_H

#include <expected>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <unordered_set>

namespace app {
class Dispatcher;
}

namespace infra::security::validation {

using json = nlohmann::json;

enum class MessageType { CMD, PING, SYSTEM, UNKNOWN };

struct ValidatedMessage {
    json message;
    MessageType type;
};

using ValidationError = std::string;
using MessageValidationResult = std::expected<ValidatedMessage, ValidationError>;

class MessageValidator {
   public:
    explicit MessageValidator(const std::unordered_set<std::string>& cmds_set);
    ~MessageValidator();

    // Message validation
    MessageValidationResult validate_message(const std::string_view& raw_message);
    std::optional<json> is_message_format_valid(const std::string_view& raw_message);
    bool is_message_size_valid(const json& message);
    bool is_message_content_safe(const json& message);

    // Content filtering
    bool contains_profanity(const std::string& text);
    bool contains_malicious_content(const std::string& text);
    bool is_sql_injection_attempt(const std::string& text);
    bool is_xss_attempt(const std::string& text);

    // Message type validation
    MessageType get_message_type(const json& message);
    bool validate_cmd_message(const json& message);

   private:
    std::unordered_set<std::string> profanity_words_;
    std::unordered_set<std::string> malicious_patterns_;
    std::unordered_set<std::string> registered_commands_;

    // Validation settings
    static constexpr int MAX_MESSAGE_SIZE = 256 * 1024;  // 256KB

    // Helper methods
    void load_profanity_filter();
    void load_security_patterns();
    std::string sanitize_string(const std::string& input);
    bool contains_pattern(const std::string& text, const std::unordered_set<std::string>& patterns);
};

}  // namespace infra::security::validation

#endif  // INFRA_SECURITY_VALIDATION_MESSAGEVALIDATOR_H
