#ifndef INFRA_SECURITY_VALIDATION_PROTO_MESSAGE_VALIDATOR_H
#define INFRA_SECURITY_VALIDATION_PROTO_MESSAGE_VALIDATOR_H

#include "proto/chat.pb.h"
#include "proto/envelope.pb.h"

#include <expected>
#include <string_view>
#include <unordered_set>

namespace infra::security::validation {

using ValidationError = std::string;

/**
 * Protobuf semantic validator.
 *
 * Guarantees:
 * - Envelope is valid and supported
 * - Payload conforms to schema
 * - Payload respects product rules (size, content)
 *
 */
class ProtoMessageValidator {
   public:
    ProtoMessageValidator();
    ~ProtoMessageValidator() = default;

    std::expected<void, ValidationError> validate_envelope(const sercom::protocol::Envelope& env);

   private:
    // Per-message validators
    std::expected<void, ValidationError> validate_chat(
        const sercom::protocol::chat::ChatMessage& msg);

    // Abuse / moderation helpers
    bool contains_profanity(std::string_view text) const;
    bool contains_pattern(std::string_view text,
                          const std::unordered_set<std::string>& patterns) const;

    void load_profanity_filter();

   private:
    std::unordered_set<std::string> profanity_words_;
    static constexpr size_t MAX_CHAT_LENGTH = 4000;
};

}  // namespace infra::security::validation

#endif  // INFRA_SECURITY_VALIDATION_PROTO_MESSAGE_VALIDATOR_H
