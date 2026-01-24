#ifndef INFRA_SECURITY_VALIDATION_PROTO_MESSAGE_VALIDATOR_H
#define INFRA_SECURITY_VALIDATION_PROTO_MESSAGE_VALIDATOR_H

#include "proto/command/activity.pb.h"
#include "proto/command/channel.pb.h"
#include "proto/command/hub.pb.h"
#include "proto/command/message.pb.h"
#include "proto/command/session.pb.h"
#include "proto/command/user.pb.h"
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
    std::expected<void, ValidationError> validate_send_message(
        const sercom::protocol::command::SendMessage& msg);
    std::expected<void, ValidationError> validate_fetch_latest_messages(
        const sercom::protocol::command::FetchLatestMessages& msg);
    std::expected<void, ValidationError> validate_fetch_messages_before(
        const sercom::protocol::command::FetchMessagesBefore& msg);
    std::expected<void, ValidationError> validate_create_hub(
        const sercom::protocol::command::CreateHub& msg);
    std::expected<void, ValidationError> validate_create_channel(
        const sercom::protocol::command::CreateChannel& msg);
    std::expected<void, ValidationError> validate_update_channel(
        const sercom::protocol::command::UpdateChannel& msg);
    std::expected<void, ValidationError> validate_join_hub(
        const sercom::protocol::command::JoinHub& msg);
    std::expected<void, ValidationError> validate_create_hub_join_code(
        const sercom::protocol::command::CreateHubJoinCode& msg);
    std::expected<void, ValidationError> validate_leave_hub(
        const sercom::protocol::command::LeaveHub& msg);
    std::expected<void, ValidationError> validate_remove_hub(
        const sercom::protocol::command::RemoveHub& msg);
    std::expected<void, ValidationError> validate_rename_hub(
        const sercom::protocol::command::RenameHub& msg);
    std::expected<void, ValidationError> validate_update_hub(
        const sercom::protocol::command::UpdateHub& msg);
    std::expected<void, ValidationError> validate_update_user(
        const sercom::protocol::command::UpdateUser& msg);
    std::expected<void, ValidationError> validate_ping(const sercom::protocol::system::Ping& msg);
};

}  // namespace infra::security::validation

#endif  // INFRA_SECURITY_VALIDATION_PROTO_MESSAGE_VALIDATOR_H
