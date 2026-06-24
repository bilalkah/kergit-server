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
#include "proto/domain/channel.pb.h"
#include "proto/domain/hub.pb.h"
#include "proto/domain/message.pb.h"
#include "proto/domain/ref.pb.h"
#include "proto/domain/user.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"
#include "proto/event/heartbeat.pb.h"
#include "proto/event/realtime.pb.h"
#include "proto/event/state.pb.h"

#include <cstdint>
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

net::outbound::OutgoingMessage make_outgoing_message(net::outbound::Target target,
                                                     std::string bytes);

// Reliable (at-least-once) variant: the wire bytes are delivered with per-connection
// sequencing, server-side buffering, and retransmit-until-acked. Use for events whose
// loss would leave the client silently out of sync while still connected (e.g. voice
// mute/deafen/join/leave). `bytes` must be a base Envelope WITHOUT a seq field.
net::outbound::OutgoingMessage make_reliable_outgoing_message(net::outbound::Target target,
                                                              std::string bytes);
net::outbound::OutgoingMessage make_command_error(const GlobalConnId& conn,
                                                  sercom::protocol::Envelope::Type type,
                                                  sercom::protocol::event::CommandErrorCode code,
                                                  std::string_view message);

// Helper to create a command drop connection outbound message
net::outbound::OutgoingMessage make_drop_connection(const GlobalConnId& conn,
                                                    sercom::protocol::event::CommandErrorCode code,
                                                    std::string_view reason);
std::string sanitize(std::string value);
std::string normalize_name_lowercase(std::string value);

// Number of UTF-8 code points in a string (matches PostgreSQL char_length()).
std::size_t utf8_length(std::string_view value);

// Maps a known channel write DB error (constraint/trigger text or SQLSTATE) to a
// clean user-facing message. Returns nullopt for unknown errors so the caller can
// fall back to a generic message instead of leaking raw DB output.
std::optional<std::string> map_channel_write_error(std::string_view what);

// Maps a known hub write DB error (constraint/trigger text) to a clean
// user-facing message. Returns nullopt for unknown errors.
std::optional<std::string> map_hub_write_error(std::string_view what);

struct ChannelScope {
    HubId hub_id;
    ChannelId channel_id;
};

sercom::protocol::domain::ChannelRef to_proto_channel_ref(const HubId& hub_id,
                                                          const ChannelId& channel_id);
std::optional<ChannelScope> to_channel_scope(const sercom::protocol::domain::ChannelRef& ref);
sercom::protocol::domain::User to_proto_user(const User& user);
sercom::protocol::domain::Hub to_proto_hub(const Hub& hub);
sercom::protocol::domain::HubMember to_proto_hub_member(const UserId& user_id,
                                                        std::optional<Role> role, bool is_online);
sercom::protocol::domain::Channel to_proto_channel(const Channel& channel);
sercom::protocol::domain::Message to_proto_message(const Message& msg);
sercom::protocol::event::MessageState to_proto_message_state(const Message& msg);

std::string make_state_sync(const sercom::protocol::event::StateSync& payload);
std::string make_state_delta(const sercom::protocol::event::StateDelta& payload);
std::string make_rt_signal(const sercom::protocol::event::RtSignal& payload);
std::string make_pong();

// app/hub/channel event packaging ---------------------------

sercom::protocol::domain::HubRole to_proto_hub_role(std::optional<Role> role);
sercom::protocol::domain::ChannelType to_proto_channel_type(ChannelType type);
ChannelType from_proto_channel_type(sercom::protocol::domain::ChannelType type);
Role from_proto_hub_role(sercom::protocol::domain::HubRole role);

// app/commands/message --------------------------------------

constexpr int kDefaultLimit = 50;
constexpr int kMaxLimit = 100;

int clamp_limit(uint32_t limit);

}  // namespace app

#endif  // APP_COMMANDS_UTILS_H
