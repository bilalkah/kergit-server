#ifndef PROTO_PARSEDPAYLOAD_H
#define PROTO_PARSEDPAYLOAD_H

#include "proto/command/activity.pb.h"
#include "proto/command/channel.pb.h"
#include "proto/command/heartbeat.pb.h"
#include "proto/command/hub.pb.h"
#include "proto/command/message.pb.h"
#include "proto/command/session.pb.h"
#include "proto/command/user.pb.h"
#include "proto/envelope.pb.h"

#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace sercom::protocol {

using ParsedPayload = std::variant<std::monostate, command::Authenticate,
                                   command::RequestStateSync, command::Typing,
                                   command::SelectActiveChannel,
                                   command::JoinVoiceChannelRequest,
                                   command::VoiceChannelActivity, command::SendMessage,
                                   command::FetchMessagesBefore, command::CreateHub,
                                   command::JoinHub, command::CreateHubJoinCode,
                                   command::LeaveHub, command::RemoveHub,
                                   command::UpdateHub, command::CreateChannel,
                                   command::UpdateChannel, command::RemoveChannel,
                                   command::UpdateUser, command::Ping>;

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
            return detail::parse_payload_as<command::Ping>(env, "Invalid PING payload");
        case Envelope::AUTH:
            return detail::parse_payload_as<command::Authenticate>(env, "Invalid AUTH payload");
        case Envelope::REQUEST_STATE_SYNC:
            return detail::parse_payload_as<command::RequestStateSync>(
                env, "Invalid REQUEST_STATE_SYNC payload");
        case Envelope::TYPING:
            return detail::parse_payload_as<command::Typing>(env, "Invalid TYPING payload");
        case Envelope::ACTIVE_CHANNEL:
            return detail::parse_payload_as<command::SelectActiveChannel>(
                env, "Invalid ACTIVE_CHANNEL payload");
        case Envelope::VOICE_JOIN:
            return detail::parse_payload_as<command::JoinVoiceChannelRequest>(
                env, "Invalid VOICE_JOIN payload");
        case Envelope::VOICE_ACTIVITY:
            return detail::parse_payload_as<command::VoiceChannelActivity>(
                env, "Invalid VOICE_ACTIVITY payload");
        case Envelope::MESSAGE_SEND:
            return detail::parse_payload_as<command::SendMessage>(env,
                                                                  "Invalid MESSAGE_SEND payload");
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
        case Envelope::HUB_UPDATE:
            return detail::parse_payload_as<command::UpdateHub>(env, "Invalid HUB_UPDATE payload");
        case Envelope::CHANNEL_CREATE:
            return detail::parse_payload_as<command::CreateChannel>(
                env, "Invalid CHANNEL_CREATE payload");
        case Envelope::CHANNEL_UPDATE:
            return detail::parse_payload_as<command::UpdateChannel>(
                env, "Invalid CHANNEL_UPDATE payload");
        case Envelope::CHANNEL_REMOVE:
            return detail::parse_payload_as<command::RemoveChannel>(
                env, "Invalid CHANNEL_REMOVE payload");
        case Envelope::USER_UPDATE:
            return detail::parse_payload_as<command::UpdateUser>(env, "Invalid USER_UPDATE payload");

        case Envelope::UNKNOWN:
            return ParsedPayloadResult("Unknown message type");

        case Envelope::CommandError:
        case Envelope::PONG:
        case Envelope::AUTH_OK:
        case Envelope::STATE_SYNC:
        case Envelope::STATE_DELTA:
        case Envelope::RT_SIGNAL:
        case Envelope::VOICE_TOKEN_ISSUED:
        case Envelope::VOICE_SELF_STATUS:
        case Envelope::VOICE_SELF_REVOKED:
            return ParsedPayloadResult("Server-only message type");

        default:
            return ParsedPayloadResult("Unsupported message type");
    }
}

}  // namespace sercom::protocol

#endif  // PROTO_PARSEDPAYLOAD_H
