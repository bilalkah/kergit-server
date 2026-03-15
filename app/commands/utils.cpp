#include "app/commands/utils.h"

#include "proto/envelope.pb.h"

#include <algorithm>
#include <cctype>
#include <string_view>

namespace app {
namespace {

template <typename TPayload>
std::string serialize_as_envelope(const sercom::protocol::Envelope::Type type,
                                  const TPayload& payload) {
    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(type);
    payload.SerializeToString(env.mutable_payload());
    return env.SerializeAsString();
}

}  // namespace

net::outbound::OutgoingMessage make_outgoing_message(net::outbound::Target target,
                                                     std::string bytes) {
    return net::outbound::OutgoingMessage{
        .target = std::move(target),
        .action = net::outbound::Action{
            std::in_place_type<net::outbound::SendPayload>,
            net::outbound::SendPayload{.payload = net::outbound::Payload{std::move(bytes), true}}}};
}

net::outbound::OutgoingMessage make_command_error(const GlobalConnId& conn,
                                                  sercom::protocol::Envelope::Type type,
                                                  sercom::protocol::event::CommandErrorCode code,
                                                  std::string_view message) {
    sercom::protocol::event::CommandError err;
    err.set_command_type(type);
    err.set_code(code);
    if (!message.empty()) {
        err.set_message(message.data(), message.size());
    }

    return make_outgoing_message(net::outbound::Target::one(conn),
                                 serialize_as_envelope(sercom::protocol::Envelope::CommandError,
                                                       err));
}

net::outbound::OutgoingMessage make_drop_connection(const GlobalConnId& conn,
                                                    sercom::protocol::event::CommandErrorCode code,
                                                    std::string_view reason) {
    return net::outbound::OutgoingMessage{
        .target = net::outbound::Target::one(conn),
        .action = net::outbound::Action{std::in_place_type<net::outbound::DropConnection>,
                                        static_cast<int>(code),
                                        std::string(reason.data(), reason.size())}};
}

std::string sanitize(std::string value) {
    value.erase(value.begin(), std::find_if(value.begin(), value.end(),
                                            [](unsigned char ch) { return !std::isspace(ch); }));
    value.erase(std::find_if(value.rbegin(), value.rend(),
                             [](unsigned char ch) { return !std::isspace(ch); })
                    .base(),
                value.end());
    return value;
}

std::string normalize_name_lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

sercom::protocol::domain::ChannelRef to_proto_channel_ref(const HubId& hub_id,
                                                          const ChannelId& channel_id) {
    sercom::protocol::domain::ChannelRef out;
    out.set_hub_id(hub_id.value);
    out.set_channel_id(channel_id.value);
    return out;
}

std::optional<ChannelScope> to_channel_scope(const sercom::protocol::domain::ChannelRef& ref) {
    if (ref.hub_id().empty() || ref.channel_id().empty()) {
        return std::nullopt;
    }
    return ChannelScope{HubId{ref.hub_id()}, ChannelId{ref.channel_id()}};
}

sercom::protocol::domain::User to_proto_user(const User& user) {
    sercom::protocol::domain::User out;
    out.set_id(user.id.value);
    out.mutable_metadata()->set_username(user.username);
    out.mutable_metadata()->set_avatar_seed(user.avatar_seed);
    return out;
}

sercom::protocol::domain::Hub to_proto_hub(const Hub& hub) {
    sercom::protocol::domain::Hub out;
    out.set_id(hub.id.value);
    out.set_name(hub.name);
    out.mutable_metadata()->set_avatar_seed(hub.avatar_seed);
    return out;
}

sercom::protocol::domain::HubMember to_proto_hub_member(const UserId& user_id,
                                                        std::optional<Role> role,
                                                        bool is_online) {
    sercom::protocol::domain::HubMember out;
    out.set_user_id(user_id.value);
    out.set_role(to_proto_hub_role(role));
    out.set_is_online(is_online);
    return out;
}

sercom::protocol::domain::Channel to_proto_channel(const Channel& channel) {
    sercom::protocol::domain::Channel out;
    out.set_id(channel.id.value);
    out.set_type(to_proto_channel_type(channel.type));
    out.mutable_metadata()->set_name(channel.name);
    return out;
}

sercom::protocol::domain::Message to_proto_message(const Message& msg) {
    sercom::protocol::domain::Message out;
    out.set_id(msg.id.value);
    out.set_author_id(msg.sender_id.value);
    out.set_content(msg.text);
    out.set_created_at_unix_us(msg.created_at_unix_us);
    return out;
}

sercom::protocol::event::MessageState to_proto_message_state(const Message& msg) {
    sercom::protocol::event::MessageState state;
    *state.mutable_message() = to_proto_message(msg);
    return state;
}

std::string make_state_sync(const sercom::protocol::event::StateSync& payload) {
    return serialize_as_envelope(sercom::protocol::Envelope::STATE_SYNC, payload);
}

std::string make_state_delta(const sercom::protocol::event::StateDelta& payload) {
    return serialize_as_envelope(sercom::protocol::Envelope::STATE_DELTA, payload);
}

std::string make_rt_signal(const sercom::protocol::event::RtSignal& payload) {
    return serialize_as_envelope(sercom::protocol::Envelope::RT_SIGNAL, payload);
}

std::string make_pong() {
    sercom::protocol::event::Pong payload;
    return serialize_as_envelope(sercom::protocol::Envelope::PONG, payload);
}

sercom::protocol::domain::HubRole to_proto_hub_role(std::optional<Role> role) {
    using ProtoHubRole = sercom::protocol::domain::HubRole;
    if (!role.has_value()) return ProtoHubRole::HubRole_MEMBER;
    switch (*role) {
        case Role::OWNER:
            return ProtoHubRole::HubRole_OWNER;
        case Role::ADMIN:
            return ProtoHubRole::HubRole_ADMIN;
        case Role::USER:
        default:
            return ProtoHubRole::HubRole_MEMBER;
    }
}

sercom::protocol::domain::ChannelType to_proto_channel_type(ChannelType type) {
    using ProtoChannelType = sercom::protocol::domain::ChannelType;
    switch (type) {
        case ChannelType::VOICE:
            return ProtoChannelType::ChannelType_VOICE;
        case ChannelType::CHAT:
        default:
            return ProtoChannelType::ChannelType_TEXT;
    }
}

ChannelType from_proto_channel_type(sercom::protocol::domain::ChannelType type) {
    using ProtoChannelType = sercom::protocol::domain::ChannelType;
    switch (type) {
        case ProtoChannelType::ChannelType_VOICE:
            return ChannelType::VOICE;
        case ProtoChannelType::ChannelType_TEXT:
        case ProtoChannelType::ChannelType_UNSPECIFIED:
        default:
            return ChannelType::CHAT;
    }
}

Role from_proto_hub_role(sercom::protocol::domain::HubRole role) {
    using ProtoHubRole = sercom::protocol::domain::HubRole;
    switch (role) {
        case ProtoHubRole::HubRole_OWNER:
            return Role::OWNER;
        case ProtoHubRole::HubRole_ADMIN:
            return Role::ADMIN;
        case ProtoHubRole::HubRole_MEMBER:
        case ProtoHubRole::HubRole_UNSPECIFIED:
        default:
            return Role::USER;
    }
}

int clamp_limit(uint32_t limit) {
    if (limit == 0) return kDefaultLimit;
    return std::min(static_cast<int>(limit), kMaxLimit);
}

}  // namespace app
