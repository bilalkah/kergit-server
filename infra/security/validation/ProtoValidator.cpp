#include "infra/security/validation/ProtoValidator.h"

#include <utility>

namespace infra::security::validation {

// ---------- entry point ----------

std::expected<void, ValidationError> ProtoMessageValidator::validate_envelope(
    const sercom::protocol::Envelope& env) {
    auto parsed = parse_and_validate(env);
    if (!parsed.has_value()) {
        return std::unexpected(parsed.error());
    }
    return {};
}

std::expected<sercom::protocol::ParsedPayload, ValidationError>
ProtoMessageValidator::parse_and_validate(const sercom::protocol::Envelope& env) {
    if (env.version() != 1) {
        return std::unexpected("Unsupported protocol version");
    }

    switch (env.type()) {
        case sercom::protocol::Envelope::PING: {
            sercom::protocol::system::Ping ping;
            if (!ping.ParseFromArray(env.payload().data(), env.payload().size())) {
                return std::unexpected("Invalid PING payload");
            }
            auto res = validate_ping(ping);
            if (!res.has_value()) return std::unexpected(res.error());
            return sercom::protocol::ParsedPayload{std::move(ping)};
        }

        case sercom::protocol::Envelope::AUTH: {
            sercom::protocol::command::Authenticate auth;
            if (!auth.ParseFromArray(env.payload().data(), env.payload().size())) {
                return std::unexpected("Invalid AUTH payload");
            }
            auto res = validate_authenticate(auth);
            if (!res.has_value()) return std::unexpected(res.error());
            return sercom::protocol::ParsedPayload{std::move(auth)};
        }

        case sercom::protocol::Envelope::TYPING: {
            sercom::protocol::command::Typing typing;
            if (!typing.ParseFromArray(env.payload().data(), env.payload().size())) {
                return std::unexpected("Invalid TYPING payload");
            }
            auto res = validate_typing(typing);
            if (!res.has_value()) return std::unexpected(res.error());
            return sercom::protocol::ParsedPayload{std::move(typing)};
        }

        case sercom::protocol::Envelope::ACTIVE_CHANNEL: {
            sercom::protocol::command::SelectActiveChannel active_channel;
            if (!active_channel.ParseFromArray(env.payload().data(), env.payload().size())) {
                return std::unexpected("Invalid ACTIVE_CHANNEL payload");
            }
            auto res = validate_active_channel(active_channel);
            if (!res.has_value()) return std::unexpected(res.error());
            return sercom::protocol::ParsedPayload{std::move(active_channel)};
        }
        case sercom::protocol::Envelope::VOICE_JOIN: {
            sercom::protocol::command::VoiceChannelMembership membership;
            if (!membership.ParseFromArray(env.payload().data(), env.payload().size())) {
                return std::unexpected("Invalid VOICE_JOIN payload");
            }
            auto res = validate_voice_channel_membership(membership);
            if (!res.has_value()) return std::unexpected(res.error());
            return sercom::protocol::ParsedPayload{std::move(membership)};
        }
        case sercom::protocol::Envelope::VOICE_ACTIVITY: {
            sercom::protocol::command::VoiceChannelActivity activity;
            if (!activity.ParseFromArray(env.payload().data(), env.payload().size())) {
                return std::unexpected("Invalid VOICE_ACTIVITY payload");
            }
            auto res = validate_voice_channel_activity(activity);
            if (!res.has_value()) return std::unexpected(res.error());
            return sercom::protocol::ParsedPayload{std::move(activity)};
        }
        case sercom::protocol::Envelope::MESSAGE_SEND: {
            sercom::protocol::command::SendMessage send_message;
            if (!send_message.ParseFromArray(env.payload().data(), env.payload().size())) {
                return std::unexpected("Invalid MESSAGE_SEND payload");
            }
            auto res = validate_send_message(send_message);
            if (!res.has_value()) return std::unexpected(res.error());
            return sercom::protocol::ParsedPayload{std::move(send_message)};
        }
        case sercom::protocol::Envelope::MESSAGE_FETCH_LATEST: {
            sercom::protocol::command::FetchLatestMessages fetch_latest;
            if (!fetch_latest.ParseFromArray(env.payload().data(), env.payload().size())) {
                return std::unexpected("Invalid MESSAGE_FETCH_LATEST payload");
            }
            auto res = validate_fetch_latest_messages(fetch_latest);
            if (!res.has_value()) return std::unexpected(res.error());
            return sercom::protocol::ParsedPayload{std::move(fetch_latest)};
        }
        case sercom::protocol::Envelope::MESSAGE_FETCH_BEFORE: {
            sercom::protocol::command::FetchMessagesBefore fetch_before;
            if (!fetch_before.ParseFromArray(env.payload().data(), env.payload().size())) {
                return std::unexpected("Invalid MESSAGE_FETCH_BEFORE payload");
            }
            auto res = validate_fetch_messages_before(fetch_before);
            if (!res.has_value()) return std::unexpected(res.error());
            return sercom::protocol::ParsedPayload{std::move(fetch_before)};
        }
        case sercom::protocol::Envelope::HUB_CREATE: {
            sercom::protocol::command::CreateHub create_hub;
            if (!create_hub.ParseFromArray(env.payload().data(), env.payload().size())) {
                return std::unexpected("Invalid HUB_CREATE payload");
            }
            auto res = validate_create_hub(create_hub);
            if (!res.has_value()) return std::unexpected(res.error());
            return sercom::protocol::ParsedPayload{std::move(create_hub)};
        }
        case sercom::protocol::Envelope::CHANNEL_CREATE: {
            sercom::protocol::command::CreateChannel create_channel;
            if (!create_channel.ParseFromArray(env.payload().data(), env.payload().size())) {
                return std::unexpected("Invalid CHANNEL_CREATE payload");
            }
            auto res = validate_create_channel(create_channel);
            if (!res.has_value()) return std::unexpected(res.error());
            return sercom::protocol::ParsedPayload{std::move(create_channel)};
        }
        case sercom::protocol::Envelope::CHANNEL_RENAME: {
            sercom::protocol::command::UpdateChannel update_channel;
            if (!update_channel.ParseFromArray(env.payload().data(), env.payload().size())) {
                return std::unexpected("Invalid CHANNEL_RENAME payload");
            }
            auto res = validate_update_channel(update_channel);
            if (!res.has_value()) return std::unexpected(res.error());
            return sercom::protocol::ParsedPayload{std::move(update_channel)};
        }
        case sercom::protocol::Envelope::CHANNEL_REMOVE: {
            sercom::protocol::command::RemoveChannel remove_channel;
            if (!remove_channel.ParseFromArray(env.payload().data(), env.payload().size())) {
                return std::unexpected("Invalid CHANNEL_REMOVE payload");
            }
            auto res = validate_remove_channel(remove_channel);
            if (!res.has_value()) return std::unexpected(res.error());
            return sercom::protocol::ParsedPayload{std::move(remove_channel)};
        }
        case sercom::protocol::Envelope::HUB_JOIN: {
            sercom::protocol::command::JoinHub join_hub;
            if (!join_hub.ParseFromArray(env.payload().data(), env.payload().size())) {
                return std::unexpected("Invalid HUB_JOIN payload");
            }
            auto res = validate_join_hub(join_hub);
            if (!res.has_value()) return std::unexpected(res.error());
            return sercom::protocol::ParsedPayload{std::move(join_hub)};
        }
        case sercom::protocol::Envelope::HUB_CREATE_JOIN_CODE: {
            sercom::protocol::command::CreateHubJoinCode create_join_code;
            if (!create_join_code.ParseFromArray(env.payload().data(), env.payload().size())) {
                return std::unexpected("Invalid HUB_CREATE_JOIN_CODE payload");
            }
            auto res = validate_create_hub_join_code(create_join_code);
            if (!res.has_value()) return std::unexpected(res.error());
            return sercom::protocol::ParsedPayload{std::move(create_join_code)};
        }
        case sercom::protocol::Envelope::HUB_LEAVE: {
            sercom::protocol::command::LeaveHub leave_hub;
            if (!leave_hub.ParseFromArray(env.payload().data(), env.payload().size())) {
                return std::unexpected("Invalid HUB_LEAVE payload");
            }
            auto res = validate_leave_hub(leave_hub);
            if (!res.has_value()) return std::unexpected(res.error());
            return sercom::protocol::ParsedPayload{std::move(leave_hub)};
        }
        case sercom::protocol::Envelope::HUB_REMOVE: {
            sercom::protocol::command::RemoveHub remove_hub;
            if (!remove_hub.ParseFromArray(env.payload().data(), env.payload().size())) {
                return std::unexpected("Invalid HUB_REMOVE payload");
            }
            auto res = validate_remove_hub(remove_hub);
            if (!res.has_value()) return std::unexpected(res.error());
            return sercom::protocol::ParsedPayload{std::move(remove_hub)};
        }
        case sercom::protocol::Envelope::HUB_RENAME: {
            sercom::protocol::command::RenameHub rename_hub;
            if (!rename_hub.ParseFromArray(env.payload().data(), env.payload().size())) {
                return std::unexpected("Invalid HUB_RENAME payload");
            }
            auto res = validate_rename_hub(rename_hub);
            if (!res.has_value()) return std::unexpected(res.error());
            return sercom::protocol::ParsedPayload{std::move(rename_hub)};
        }
        case sercom::protocol::Envelope::HUB_UPDATE: {
            sercom::protocol::command::UpdateHub update_hub;
            if (!update_hub.ParseFromArray(env.payload().data(), env.payload().size())) {
                return std::unexpected("Invalid HUB_UPDATE payload");
            }
            auto res = validate_update_hub(update_hub);
            if (!res.has_value()) return std::unexpected(res.error());
            return sercom::protocol::ParsedPayload{std::move(update_hub)};
        }
        case sercom::protocol::Envelope::USER_UPDATE: {
            sercom::protocol::command::UpdateUser update_user;
            if (!update_user.ParseFromArray(env.payload().data(), env.payload().size())) {
                return std::unexpected("Invalid USER_UPDATE payload");
            }
            auto res = validate_update_user(update_user);
            if (!res.has_value()) return std::unexpected(res.error());
            return sercom::protocol::ParsedPayload{std::move(update_user)};
        }

        case sercom::protocol::Envelope::UNKNOWN:
            return std::unexpected("Unknown message type");

        case sercom::protocol::Envelope::CommandError:
        case sercom::protocol::Envelope::PONG:
        case sercom::protocol::Envelope::SESSION_BOOTSTRAP:
        case sercom::protocol::Envelope::PRESENCE:
        case sercom::protocol::Envelope::MESSAGE_CREATED:
        case sercom::protocol::Envelope::MESSAGE_BATCH:
        case sercom::protocol::Envelope::HUB_CREATED:
        case sercom::protocol::Envelope::HUB_MEMBER_JOINED:
        case sercom::protocol::Envelope::HUB_MEMBER_LEFT:
        case sercom::protocol::Envelope::HUB_REMOVED:
        case sercom::protocol::Envelope::HUB_JOIN_CODE_CREATED:
        case sercom::protocol::Envelope::HUB_RENAMED:
        case sercom::protocol::Envelope::HUB_AVATAR_CHANGED:
        case sercom::protocol::Envelope::USER_PROFILE_UPDATED:
        case sercom::protocol::Envelope::CHANNEL_CREATED:
        case sercom::protocol::Envelope::CHANNEL_RENAMED:
        case sercom::protocol::Envelope::CHANNEL_REMOVED:
        case sercom::protocol::Envelope::VOICE_TOKEN_ISSUED:
        case sercom::protocol::Envelope::VOICE_CHANNEL_PARTICIPANTS:
        case sercom::protocol::Envelope::VOICE_CHANNEL_PRESENCE:
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

std::expected<void, ValidationError> ProtoMessageValidator::validate_voice_channel_membership(
    const sercom::protocol::command::VoiceChannelMembership& msg) {
    if (msg.hub_id() == 0) {
        return std::unexpected("hub_id is empty");
    }

    if (msg.channel_id() == 0) {
        return std::unexpected("channel_id is empty");
    }

    if (msg.state() == sercom::protocol::command::VoiceChannelMembership::STATE_UNSPECIFIED) {
        return std::unexpected("voice channel state is unspecified");
    }

    return {};
}

std::expected<void, ValidationError> ProtoMessageValidator::validate_voice_channel_activity(
    const sercom::protocol::command::VoiceChannelActivity& msg) {
    if (msg.hub_id() == 0) {
        return std::unexpected("hub_id is empty");
    }

    if (msg.channel_id() == 0) {
        return std::unexpected("channel_id is empty");
    }

    if (msg.state() == sercom::protocol::command::VoiceChannelActivity::STATE_UNSPECIFIED) {
        return std::unexpected("voice activity state is unspecified");
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

// ---------- HUB_CREATE validation ----------

std::expected<void, ValidationError> ProtoMessageValidator::validate_create_hub(
    const sercom::protocol::command::CreateHub& msg) {
    if (msg.name().empty()) {
        return std::unexpected("name is empty");
    }
    return {};
}

// ---------- CHANNEL_CREATE validation ----------

std::expected<void, ValidationError> ProtoMessageValidator::validate_create_channel(
    const sercom::protocol::command::CreateChannel& msg) {
    if (msg.hub_id() == 0) {
        return std::unexpected("hub_id is empty");
    }
    if (msg.name().empty()) {
        return std::unexpected("name is empty");
    }
    return {};
}

// ---------- CHANNEL_RENAME (UpdateChannel payload) validation ----------

std::expected<void, ValidationError> ProtoMessageValidator::validate_update_channel(
    const sercom::protocol::command::UpdateChannel& msg) {
    if (msg.hub_id() == 0) {
        return std::unexpected("hub_id is empty");
    }
    if (msg.channel_id() == 0) {
        return std::unexpected("channel_id is empty");
    }
    if (msg.changes().empty()) {
        return std::unexpected("changes are empty");
    }

    bool has_name = false;
    for (const auto& change : msg.changes()) {
        switch (change.change_case()) {
            case sercom::protocol::command::ChannelChange::kName:
                if (change.name().empty()) {
                    return std::unexpected("name is empty");
                }
                has_name = true;
                break;
            case sercom::protocol::command::ChannelChange::CHANGE_NOT_SET:
            default:
                return std::unexpected("invalid change");
        }
    }

    if (!has_name) {
        return std::unexpected("no supported changes");
    }

    return {};
}

std::expected<void, ValidationError> ProtoMessageValidator::validate_remove_channel(
    const sercom::protocol::command::RemoveChannel& msg) {
    if (msg.hub_id() == 0) {
        return std::unexpected("hub_id is empty");
    }

    if (msg.channel_id() == 0) {
        return std::unexpected("channel_id is empty");
    }

    return {};
}

// ---------- HUB_JOIN validation ----------

std::expected<void, ValidationError> ProtoMessageValidator::validate_join_hub(
    const sercom::protocol::command::JoinHub& msg) {
    if (msg.join_code().empty()) {
        return std::unexpected("join_code is empty");
    }
    return {};
}

// ---------- HUB_CREATE_JOIN_CODE validation ----------

std::expected<void, ValidationError> ProtoMessageValidator::validate_create_hub_join_code(
    const sercom::protocol::command::CreateHubJoinCode& msg) {
    if (msg.hub_id() == 0) {
        return std::unexpected("hub_id is empty");
    }
    return {};
}

// ---------- HUB_LEAVE validation ----------

std::expected<void, ValidationError> ProtoMessageValidator::validate_leave_hub(
    const sercom::protocol::command::LeaveHub& msg) {
    if (msg.hub_id() == 0) {
        return std::unexpected("hub_id is empty");
    }
    return {};
}

// ---------- HUB_REMOVE validation ----------

std::expected<void, ValidationError> ProtoMessageValidator::validate_remove_hub(
    const sercom::protocol::command::RemoveHub& msg) {
    if (msg.hub_id() == 0) {
        return std::unexpected("hub_id is empty");
    }
    return {};
}

// ---------- HUB_RENAME validation ----------

std::expected<void, ValidationError> ProtoMessageValidator::validate_rename_hub(
    const sercom::protocol::command::RenameHub& msg) {
    if (msg.hub_id() == 0) {
        return std::unexpected("hub_id is empty");
    }
    if (msg.name().empty()) {
        return std::unexpected("name is empty");
    }
    return {};
}

// ---------- HUB_UPDATE validation ----------

std::expected<void, ValidationError> ProtoMessageValidator::validate_update_hub(
    const sercom::protocol::command::UpdateHub& msg) {
    if (msg.hub_id() == 0) {
        return std::unexpected("hub_id is empty");
    }
    if (msg.changes().empty()) {
        return std::unexpected("changes are empty");
    }
    bool has_name = false;
    bool has_avatar = false;
    for (const auto& change : msg.changes()) {
        switch (change.change_case()) {
            case sercom::protocol::command::HubChange::kName:
                if (change.name().empty()) {
                    return std::unexpected("name is empty");
                }
                has_name = true;
                break;
            case sercom::protocol::command::HubChange::kAvatarSeed:
                if (change.avatar_seed().empty()) {
                    return std::unexpected("avatar_seed is empty");
                }
                has_avatar = true;
                break;
            case sercom::protocol::command::HubChange::CHANGE_NOT_SET:
            default:
                return std::unexpected("invalid change");
        }
    }
    if (!has_name && !has_avatar) {
        return std::unexpected("no supported changes");
    }
    return {};
}

// ---------- USER_UPDATE validation ----------

std::expected<void, ValidationError> ProtoMessageValidator::validate_update_user(
    const sercom::protocol::command::UpdateUser& msg) {
    if (msg.changes().empty()) {
        return std::unexpected("changes are empty");
    }

    bool has_username = false;
    bool has_avatar = false;
    for (const auto& change : msg.changes()) {
        switch (change.change_case()) {
            case sercom::protocol::command::UserChange::kUsername:
                if (change.username().empty()) {
                    return std::unexpected("username is empty");
                }
                has_username = true;
                break;
            case sercom::protocol::command::UserChange::kAvatarSeed:
                if (change.avatar_seed().empty()) {
                    return std::unexpected("avatar_seed is empty");
                }
                has_avatar = true;
                break;
            case sercom::protocol::command::UserChange::CHANGE_NOT_SET:
            default:
                return std::unexpected("invalid change");
        }
    }

    if (!has_username && !has_avatar) {
        return std::unexpected("no supported changes");
    }
    return {};
}

}  // namespace infra::security::validation
