#ifndef INFRA_SECURITY_VALIDATION_PROTO_MESSAGE_VALIDATOR_H
#define INFRA_SECURITY_VALIDATION_PROTO_MESSAGE_VALIDATOR_H

#include "proto/command/activity.pb.h"
#include "proto/command/session.pb.h"
#include "proto/envelope.pb.h"
#include "proto/system/heartbeat.pb.h"

#include <expected>
#include <string>

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
    ProtoMessageValidator() = default;
    ~ProtoMessageValidator() = default;

    std::expected<void, ValidationError> validate_envelope(const sercom::protocol::Envelope& env);

   private:
    // Per-message validators
    std::expected<void, ValidationError> validate_authenticate(
        const sercom::protocol::command::Authenticate& msg);
    std::expected<void, ValidationError> validate_typing(
        const sercom::protocol::command::Typing& msg);
    std::expected<void, ValidationError> validate_active_channel(
        const sercom::protocol::command::SelectActiveChannel& msg);
    std::expected<void, ValidationError> validate_ping(const sercom::protocol::system::Ping& msg);
};

}  // namespace infra::security::validation

#endif  // INFRA_SECURITY_VALIDATION_PROTO_MESSAGE_VALIDATOR_H
