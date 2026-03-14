#include "app/commands/utils.h"

#include "proto/envelope.pb.h"
#include "proto/event/channel.pb.h"
#include "proto/event/hub.pb.h"
#include "proto/event/presence.pb.h"

#include <algorithm>
#include <cctype>
#include <string_view>

namespace app {

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

    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(sercom::protocol::Envelope::CommandError);
    err.SerializeToString(env.mutable_payload());

    std::string bytes;
    env.SerializeToString(&bytes);
    return make_outgoing_message(net::outbound::Target::one(conn), std::move(bytes));
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

uint64_t to_epoch_ms(const std::chrono::system_clock::time_point& tp) {
    if (tp.time_since_epoch().count() == 0) return 0;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
    return ms > 0 ? static_cast<uint64_t>(ms) : 0;
}

sercom::protocol::domain::Message to_proto_message(const Message& msg,
                                                   const std::optional<User>& author_opt) {
    sercom::protocol::domain::Message out;
    out.set_id(msg.id.value);
    out.set_author_id(msg.sender_id.value);
    out.set_content(msg.text);
    out.set_created_at_ms(to_epoch_ms(msg.sent_at));

    if (author_opt.has_value()) {
        auto* author = out.mutable_author();
        author->set_id(author_opt->id.value);
        author->mutable_metadata()->set_username(author_opt->username);
        author->mutable_metadata()->set_avatar_seed(author_opt->avatar_seed);
    }

    return out;
}

std::string make_message_batch(const ChannelId& ch_id,
                               sercom::protocol::event::MessageBatch::Direction direction,
                               const std::vector<sercom::protocol::domain::Message>& messages) {
    sercom::protocol::event::MessageBatch batch;
    batch.set_channel_id(ch_id.value);
    batch.set_direction(direction);

    for (const auto& msg : messages) {
        *batch.add_messages() = msg;
    }

    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(sercom::protocol::Envelope::MESSAGE_BATCH);
    batch.SerializeToString(env.mutable_payload());

    std::string bytes;
    env.SerializeToString(&bytes);
    return bytes;
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

std::string make_hub_create(const HubId& hub_id, const std::string& name,
                            const std::string& avatar_seed, const UserId& self_user_id,
                            Role self_role, bool self_online,
                            const std::optional<Channel>& default_channel) {
    sercom::protocol::event::HubCreated created;
    created.mutable_hub()->set_id(hub_id.value);
    created.mutable_hub()->set_name(name);
    created.mutable_hub()->mutable_metadata()->set_avatar_seed(avatar_seed);

    auto* self = created.mutable_self_member();
    self->set_hub_id(hub_id.value);
    self->set_user_id(self_user_id.value);
    self->set_is_online(self_online);
    self->set_role(to_proto_hub_role(self_role));

    if (default_channel.has_value()) {
        auto* ch = created.mutable_channel();
        ch->set_id(default_channel->id.value);
        ch->set_hub_id(hub_id.value);
        ch->set_type(to_proto_channel_type(default_channel->type));
        ch->mutable_metadata()->set_name(default_channel->name);
    }

    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(sercom::protocol::Envelope::HUB_CREATED);
    created.SerializeToString(env.mutable_payload());
    return env.SerializeAsString();
}

std::string make_hub_already_member(const HubId& hub_id, const UserId& user_id, Role role,
                                    bool is_online) {
    sercom::protocol::event::HubAlreadyMember already;
    auto* member = already.mutable_self_member();
    member->set_hub_id(hub_id.value);
    member->set_user_id(user_id.value);
    member->set_is_online(is_online);
    member->set_role(to_proto_hub_role(role));

    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(sercom::protocol::Envelope::HUB_ALREADY_MEMBER);
    already.SerializeToString(env.mutable_payload());
    return env.SerializeAsString();
}

std::string make_hub_update(const HubId& hub_id, const std::string& name,
                            const std::string& avatar_seed) {
    sercom::protocol::event::HubUpdated updated;
    updated.set_hub_id(hub_id.value);
    updated.set_action(sercom::protocol::event::HUB_ACTION_UPDATED);
    auto* hub = updated.mutable_hub();
    hub->set_id(hub_id.value);
    hub->set_name(name);
    hub->mutable_metadata()->set_avatar_seed(avatar_seed);

    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(sercom::protocol::Envelope::HUB_UPDATED);
    updated.SerializeToString(env.mutable_payload());
    return env.SerializeAsString();
}

std::string make_hub_remove(const HubId& hub_id) {
    sercom::protocol::event::HubUpdated updated;
    updated.set_hub_id(hub_id.value);
    updated.set_action(sercom::protocol::event::HUB_ACTION_REMOVED);
    auto* hub = updated.mutable_hub();
    hub->set_id(hub_id.value);

    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(sercom::protocol::Envelope::HUB_UPDATED);
    updated.SerializeToString(env.mutable_payload());
    return env.SerializeAsString();
}

std::string make_member_join(const HubId& hub_id, const UserId& user_id, Role role,
                             const std::string& username, const std::string& avatar_seed,
                             bool is_online) {
    sercom::protocol::event::HubMemberJoined joined;
    auto* member = joined.mutable_member();
    member->set_hub_id(hub_id.value);
    member->set_user_id(user_id.value);
    member->set_is_online(is_online);
    member->set_role(to_proto_hub_role(role));

    auto* user = joined.mutable_user();
    user->set_id(user_id.value);
    user->mutable_metadata()->set_username(username);
    user->mutable_metadata()->set_avatar_seed(avatar_seed);

    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(sercom::protocol::Envelope::HUB_MEMBER_JOINED);
    joined.SerializeToString(env.mutable_payload());
    return env.SerializeAsString();
}

std::string make_member_leave(const HubId& hub_id, const UserId& user_id) {
    sercom::protocol::event::HubMemberLeft left;
    left.set_hub_id(hub_id.value);
    left.set_user_id(user_id.value);

    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(sercom::protocol::Envelope::HUB_MEMBER_LEFT);
    left.SerializeToString(env.mutable_payload());
    return env.SerializeAsString();
}

std::string make_member_presence(const HubId& hub_id, const UserId& user_id, bool is_online) {
    sercom::protocol::event::PresenceEvent presence;
    auto* payload = presence.mutable_presence_changed();
    payload->set_hub_id(hub_id.value);
    payload->set_user_id(user_id.value);
    payload->set_is_online(is_online);

    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(sercom::protocol::Envelope::PRESENCE);
    presence.SerializeToString(env.mutable_payload());
    return env.SerializeAsString();
}

std::string make_channel_create(const HubId& hub_id, const Channel& channel) {
    sercom::protocol::event::ChannelUpdated updated;
    updated.set_hub_id(hub_id.value);
    updated.set_action(sercom::protocol::event::CHANNEL_ACTION_CREATED);
    auto* out_channel = updated.mutable_channel();
    out_channel->set_id(channel.id.value);
    out_channel->set_hub_id(hub_id.value);
    out_channel->set_type(to_proto_channel_type(channel.type));
    out_channel->mutable_metadata()->set_name(channel.name);

    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(sercom::protocol::Envelope::CHANNEL_UPDATED);
    updated.SerializeToString(env.mutable_payload());
    return env.SerializeAsString();
}

std::string make_channel_update(const HubId& hub_id, const Channel& channel) {
    sercom::protocol::event::ChannelUpdated updated;
    updated.set_hub_id(hub_id.value);
    updated.set_action(sercom::protocol::event::CHANNEL_ACTION_UPDATED);
    auto* out_channel = updated.mutable_channel();
    out_channel->set_id(channel.id.value);
    out_channel->set_hub_id(hub_id.value);
    out_channel->set_type(to_proto_channel_type(channel.type));
    out_channel->mutable_metadata()->set_name(channel.name);

    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(sercom::protocol::Envelope::CHANNEL_UPDATED);
    updated.SerializeToString(env.mutable_payload());
    return env.SerializeAsString();
}

std::string make_channel_remove(const HubId& hub_id, const ChannelId& channel_id) {
    sercom::protocol::event::ChannelUpdated updated;
    updated.set_hub_id(hub_id.value);
    updated.set_action(sercom::protocol::event::CHANNEL_ACTION_REMOVED);
    auto* out_channel = updated.mutable_channel();
    out_channel->set_id(channel_id.value);

    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(sercom::protocol::Envelope::CHANNEL_UPDATED);
    updated.SerializeToString(env.mutable_payload());
    return env.SerializeAsString();
}

}  // namespace app
