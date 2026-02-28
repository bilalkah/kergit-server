#ifndef PROTO_PARSEDPAYLOAD_H
#define PROTO_PARSEDPAYLOAD_H

#include "proto/command/activity.pb.h"
#include "proto/command/channel.pb.h"
#include "proto/command/hub.pb.h"
#include "proto/command/message.pb.h"
#include "proto/command/session.pb.h"
#include "proto/command/user.pb.h"
#include "proto/envelope.pb.h"
#include "proto/system/heartbeat.pb.h"

#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace sercom::protocol {

using ParsedPayload = std::variant<std::monostate, command::Authenticate, command::Typing,
                                   command::SelectActiveChannel, command::VoiceChannelMembership,
                                   command::VoiceChannelActivity, command::SendMessage,
                                   command::FetchLatestMessages, command::FetchMessagesBefore,
                                   command::CreateHub, command::JoinHub,
                                   command::CreateHubJoinCode, command::LeaveHub,
                                   command::RemoveHub, command::RenameHub,
                                   command::UpdateHub, command::CreateChannel,
                                   command::UpdateChannel, command::RemoveChannel,
                                   command::UpdateUser, system::Ping>;

class ParsedPayloadResult {
   public:
    ParsedPayloadResult(ParsedPayload payload) : payload_(std::move(payload)) {}
    ParsedPayloadResult(std::string error) : error_(std::move(error)) {}

    bool has_value() const noexcept { return error_.empty(); }
    explicit operator bool() const noexcept { return has_value(); }

    ParsedPayload& value() & { return payload_; }
    const ParsedPayload& value() const& { return payload_; }
    ParsedPayload&& value() && { return std::move(payload_); }

    const std::string& error() const& { return error_; }

   private:
    ParsedPayload payload_{};
    std::string error_;
};

namespace detail {

template <typename T>
inline ParsedPayloadResult parse_payload_as(const Envelope& env, std::string_view error) {
    T msg;
    if (!msg.ParseFromString(env.payload())) {
        return ParsedPayloadResult(std::string(error));
    }
    return ParsedPayloadResult(ParsedPayload{std::move(msg)});
}

}  // namespace detail

inline ParsedPayloadResult parse_inbound_payload(const Envelope& env) {
    if (env.version() != 1) {
        return ParsedPayloadResult("Unsupported protocol version");
    }

    switch (env.type()) {
        case Envelope::PING:
            return detail::parse_payload_as<system::Ping>(env, "Invalid PING payload");
        case Envelope::AUTH:
            return detail::parse_payload_as<command::Authenticate>(env, "Invalid AUTH payload");
        case Envelope::TYPING:
            return detail::parse_payload_as<command::Typing>(env, "Invalid TYPING payload");
        case Envelope::ACTIVE_CHANNEL:
            return detail::parse_payload_as<command::SelectActiveChannel>(
                env, "Invalid ACTIVE_CHANNEL payload");
        case Envelope::VOICE_JOIN:
            return detail::parse_payload_as<command::VoiceChannelMembership>(
                env, "Invalid VOICE_JOIN payload");
        case Envelope::VOICE_ACTIVITY:
            return detail::parse_payload_as<command::VoiceChannelActivity>(
                env, "Invalid VOICE_ACTIVITY payload");
        case Envelope::MESSAGE_SEND:
            return detail::parse_payload_as<command::SendMessage>(env, "Invalid MESSAGE_SEND payload");
        case Envelope::MESSAGE_FETCH_LATEST:
            return detail::parse_payload_as<command::FetchLatestMessages>(
                env, "Invalid MESSAGE_FETCH_LATEST payload");
        case Envelope::MESSAGE_FETCH_BEFORE:
            return detail::parse_payload_as<command::FetchMessagesBefore>(
                env, "Invalid MESSAGE_FETCH_BEFORE payload");
        case Envelope::HUB_CREATE:
            return detail::parse_payload_as<command::CreateHub>(env, "Invalid HUB_CREATE payload");
        case Envelope::HUB_JOIN:
            return detail::parse_payload_as<command::JoinHub>(env, "Invalid HUB_JOIN payload");
        case Envelope::HUB_CREATE_JOIN_CODE:
            return detail::parse_payload_as<command::CreateHubJoinCode>(
                env, "Invalid HUB_CREATE_JOIN_CODE payload");
        case Envelope::HUB_LEAVE:
            return detail::parse_payload_as<command::LeaveHub>(env, "Invalid HUB_LEAVE payload");
        case Envelope::HUB_REMOVE:
            return detail::parse_payload_as<command::RemoveHub>(env, "Invalid HUB_REMOVE payload");
        case Envelope::HUB_RENAME:
            return detail::parse_payload_as<command::RenameHub>(env, "Invalid HUB_RENAME payload");
        case Envelope::HUB_UPDATE:
            return detail::parse_payload_as<command::UpdateHub>(env, "Invalid HUB_UPDATE payload");
        case Envelope::CHANNEL_CREATE:
            return detail::parse_payload_as<command::CreateChannel>(
                env, "Invalid CHANNEL_CREATE payload");
        case Envelope::CHANNEL_RENAME:
            return detail::parse_payload_as<command::UpdateChannel>(
                env, "Invalid CHANNEL_RENAME payload");
        case Envelope::CHANNEL_REMOVE:
            return detail::parse_payload_as<command::RemoveChannel>(
                env, "Invalid CHANNEL_REMOVE payload");
        case Envelope::USER_UPDATE:
            return detail::parse_payload_as<command::UpdateUser>(env, "Invalid USER_UPDATE payload");

        case Envelope::UNKNOWN:
            return ParsedPayloadResult("Unknown message type");

        case Envelope::CommandError:
        case Envelope::PONG:
        case Envelope::SESSION_BOOTSTRAP:
        case Envelope::PRESENCE:
        case Envelope::MESSAGE_CREATED:
        case Envelope::MESSAGE_BATCH:
        case Envelope::HUB_CREATED:
        case Envelope::HUB_MEMBER_JOINED:
        case Envelope::HUB_MEMBER_LEFT:
        case Envelope::HUB_REMOVED:
        case Envelope::HUB_JOIN_CODE_CREATED:
        case Envelope::HUB_RENAMED:
        case Envelope::HUB_AVATAR_CHANGED:
        case Envelope::USER_PROFILE_UPDATED:
        case Envelope::CHANNEL_CREATED:
        case Envelope::CHANNEL_RENAMED:
        case Envelope::CHANNEL_REMOVED:
        case Envelope::VOICE_TOKEN_ISSUED:
        case Envelope::VOICE_CHANNEL_PARTICIPANTS:
        case Envelope::VOICE_CHANNEL_PRESENCE:
        case Envelope::VOICE_SELF_STATUS:
        case Envelope::VOICE_SELF_REVOKED:
        case Envelope::AUTH_OK:
            return ParsedPayloadResult("Server-only message type");

        default:
            return ParsedPayloadResult("Unsupported message type");
    }
}

}  // namespace sercom::protocol

#endif  // PROTO_PARSEDPAYLOAD_H
