#ifndef APP_COMMANDS_UTILS_H
#define APP_COMMANDS_UTILS_H

#include "app/queue/Msg.h"
#include "domains/Channel.h"
#include "domains/Hub.h"
#include "domains/Message.h"
#include "domains/User.h"
#include "domains/ids/Ids.h"
#include "net/outbound/Msg.h"
#include "utils/Logger.h"
#include "utils/Metrics.h"

// Protobuf includes
#include "proto/command/message.pb.h"
#include "proto/domain/channel.pb.h"
#include "proto/domain/hub.pb.h"
#include "proto/domain/message.pb.h"
#include "proto/domain/user.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"
#include "proto/event/message.pb.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <typeinfo>
#include <vector>

namespace app {

template <typename T>
inline const T& require_parsed(const queue::MessageEvent& event) {
    const auto* cmd = std::get_if<T>(&event.payload.parsed);
    if (cmd) {
        return *cmd;
    }
    utils::metrics::counters().parsed_payload_violation_total.fetch_add(1,
                                                                        std::memory_order_relaxed);
    utils::log_line(
        utils::LogLevel::ERROR,
        std::string("Parsed payload invariant violated for command ") + typeid(T).name());
    std::terminate();
}

inline net::outbound::OutgoingMessage make_outgoing_message(net::outbound::Target target,
                                                            std::string bytes) {
    return net::outbound::OutgoingMessage{
        .target = std::move(target),
        .action = net::outbound::Action{
            std::in_place_type<net::outbound::SendPayload>,
            net::outbound::SendPayload{.payload = net::outbound::Payload{std::move(bytes), true}}}};
}

inline net::outbound::OutgoingMessage make_command_error(
    const GlobalConnId& conn, sercom::protocol::Envelope::Type type,
    sercom::protocol::event::CommandErrorCode code, std::string_view message) {
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

// Helper to create a command drop connection outbound message
inline net::outbound::OutgoingMessage make_drop_connection(
    const GlobalConnId& conn, sercom::protocol::event::CommandErrorCode code,
    std::string_view reason) {
    return net::outbound::OutgoingMessage{
        .target = net::outbound::Target::one(conn),
        .action = net::outbound::Action{std::in_place_type<net::outbound::DropConnection>,
                                        static_cast<int>(code),
                                        std::string(reason.data(), reason.size())}};
}

inline std::string sanitize(std::string value) {
    value.erase(value.begin(), std::find_if(value.begin(), value.end(),
                                            [](unsigned char ch) { return !std::isspace(ch); }));
    value.erase(std::find_if(value.rbegin(), value.rend(),
                             [](unsigned char ch) { return !std::isspace(ch); })
                    .base(),
                value.end());
    return value;
}

inline std::string normalize_name_lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

inline uint64_t to_epoch_ms(const std::chrono::system_clock::time_point& tp) {
    if (tp.time_since_epoch().count() == 0) return 0;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
    return ms > 0 ? static_cast<uint64_t>(ms) : 0;
}

inline sercom::protocol::domain::Message to_proto_message(const Message& msg,
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

inline std::string make_message_batch(
    const ChannelId& ch_id, sercom::protocol::event::MessageBatch::Direction direction,
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

inline sercom::protocol::domain::HubRole to_proto_hub_role(std::optional<Role> role) {
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

inline sercom::protocol::domain::ChannelType to_proto_channel_type(ChannelType type) {
    using ProtoChannelType = sercom::protocol::domain::ChannelType;
    switch (type) {
        case ChannelType::VOICE:
            return ProtoChannelType::ChannelType_VOICE;
        case ChannelType::CHAT:
        default:
            return ProtoChannelType::ChannelType_TEXT;
    }
}

inline ChannelType from_proto_channel_type(sercom::protocol::domain::ChannelType type) {
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

inline Role from_proto_hub_role(sercom::protocol::domain::HubRole role) {
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

// app/commands/message --------------------------------------

constexpr int kDefaultLimit = 50;
constexpr int kMaxLimit = 100;

static inline int clamp_limit(uint32_t limit) {
    if (limit == 0) return kDefaultLimit;
    return std::min(static_cast<int>(limit), kMaxLimit);
}

}  // namespace app

#endif  // APP_COMMANDS_UTILS_H
