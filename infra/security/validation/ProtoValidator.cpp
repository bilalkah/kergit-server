#include "infra/security/validation/ProtoValidator.h"

namespace infra::security::validation {

// ---------- entry point ----------

std::expected<void, ValidationError> ProtoMessageValidator::validate_envelope(
    const sercom::protocol::Envelope& env) {
    if (env.version() != 1) {
        return std::unexpected("Unsupported protocol version");
    }

    switch (env.type()) {
        case sercom::protocol::Envelope::PING: {
            sercom::protocol::system::Ping ping;
            if (!ping.ParseFromArray(env.payload().data(), env.payload().size())) {
                return std::unexpected("Invalid PING payload");
            }
            return validate_ping(ping);
        }

        case sercom::protocol::Envelope::AUTH: {
            sercom::protocol::command::Authenticate auth;
            if (!auth.ParseFromArray(env.payload().data(), env.payload().size())) {
                return std::unexpected("Invalid AUTH payload");
            }
            return validate_authenticate(auth);
        }

        case sercom::protocol::Envelope::TYPING: {
            sercom::protocol::command::Typing typing;
            if (!typing.ParseFromArray(env.payload().data(), env.payload().size())) {
                return std::unexpected("Invalid TYPING payload");
            }
            return validate_typing(typing);
        }

        case sercom::protocol::Envelope::ACTIVE_CHANNEL: {
            sercom::protocol::command::SelectActiveChannel active_channel;
            if (!active_channel.ParseFromArray(env.payload().data(), env.payload().size())) {
                return std::unexpected("Invalid ACTIVE_CHANNEL payload");
            }
            return validate_active_channel(active_channel);
        }
        case sercom::protocol::Envelope::MESSAGE_SEND: {
            sercom::protocol::command::SendMessage send_message;
            if (!send_message.ParseFromArray(env.payload().data(), env.payload().size())) {
                return std::unexpected("Invalid MESSAGE_SEND payload");
            }
            return validate_send_message(send_message);
        }
        case sercom::protocol::Envelope::MESSAGE_FETCH_LATEST: {
            sercom::protocol::command::FetchLatestMessages fetch_latest;
            if (!fetch_latest.ParseFromArray(env.payload().data(), env.payload().size())) {
                return std::unexpected("Invalid MESSAGE_FETCH_LATEST payload");
            }
            return validate_fetch_latest_messages(fetch_latest);
        }
        case sercom::protocol::Envelope::MESSAGE_FETCH_BEFORE: {
            sercom::protocol::command::FetchMessagesBefore fetch_before;
            if (!fetch_before.ParseFromArray(env.payload().data(), env.payload().size())) {
                return std::unexpected("Invalid MESSAGE_FETCH_BEFORE payload");
            }
            return validate_fetch_messages_before(fetch_before);
        }

        case sercom::protocol::Envelope::UNKNOWN:
            return std::unexpected("Unknown message type");

        case sercom::protocol::Envelope::CommandError:
        case sercom::protocol::Envelope::PONG:
        case sercom::protocol::Envelope::SESSION_BOOTSTRAP:
        case sercom::protocol::Envelope::PRESENCE:
        case sercom::protocol::Envelope::MESSAGE_CREATED:
        case sercom::protocol::Envelope::MESSAGE_BATCH:
            return std::unexpected("Server-only message type");

        default:
            return std::unexpected("Unsupported message type");
    }
}

// ---------- PING validation ----------

std::expected<void, ValidationError> ProtoMessageValidator::validate_ping(
    const sercom::protocol::system::Ping& msg) {
    if (msg.client_ts_ms() == 0) {
        return std::unexpected("client_ts_ms is empty");
    }
    return {};
}

// ---------- AUTH validation ----------

std::expected<void, ValidationError> ProtoMessageValidator::validate_authenticate(
    const sercom::protocol::command::Authenticate& msg) {
    if (msg.type() == sercom::protocol::command::AuthType_UNSPECIFIED) {
        return std::unexpected("auth type is unspecified");
    }

    if (msg.provider() == sercom::protocol::command::AuthProvider_UNSPECIFIED) {
        return std::unexpected("auth provider is unspecified");
    }

    if (msg.token().empty()) {
        return std::unexpected("token is empty");
    }

    return {};
}

// ---------- TYPING validation ----------

std::expected<void, ValidationError> ProtoMessageValidator::validate_typing(
    const sercom::protocol::command::Typing& msg) {
    if (msg.hub_id() == 0) {
        return std::unexpected("hub_id is empty");
    }

    if (msg.channel_id() == 0) {
        return std::unexpected("channel_id is empty");
    }

    if (msg.state() == sercom::protocol::command::Typing::STATE_UNSPECIFIED) {
        return std::unexpected("typing state is unspecified");
    }

    return {};
}

// ---------- ACTIVE_CHANNEL validation ----------

std::expected<void, ValidationError> ProtoMessageValidator::validate_active_channel(
    const sercom::protocol::command::SelectActiveChannel& msg) {
    if (msg.hub_id() == 0) {
        return std::unexpected("hub_id is empty");
    }

    if (msg.channel_id() == 0) {
        return std::unexpected("channel_id is empty");
    }

    return {};
}

// ---------- MESSAGE_SEND validation ----------

std::expected<void, ValidationError> ProtoMessageValidator::validate_send_message(
    const sercom::protocol::command::SendMessage& msg) {
    if (msg.hub_id() == 0) {
        return std::unexpected("hub_id is empty");
    }

    if (msg.channel_id() == 0) {
        return std::unexpected("channel_id is empty");
    }

    if (msg.content().empty()) {
        return std::unexpected("content is empty");
    }

    return {};
}

// ---------- MESSAGE_FETCH_LATEST validation ----------

std::expected<void, ValidationError> ProtoMessageValidator::validate_fetch_latest_messages(
    const sercom::protocol::command::FetchLatestMessages& msg) {
    if (msg.hub_id() == 0) {
        return std::unexpected("hub_id is empty");
    }

    if (msg.channel_id() == 0) {
        return std::unexpected("channel_id is empty");
    }

    return {};
}

// ---------- MESSAGE_FETCH_BEFORE validation ----------

std::expected<void, ValidationError> ProtoMessageValidator::validate_fetch_messages_before(
    const sercom::protocol::command::FetchMessagesBefore& msg) {
    if (msg.hub_id() == 0) {
        return std::unexpected("hub_id is empty");
    }

    if (msg.channel_id() == 0) {
        return std::unexpected("channel_id is empty");
    }

    if (msg.before_message_id() == 0) {
        return std::unexpected("before_message_id is empty");
    }

    return {};
}

}  // namespace infra::security::validation
