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

#include <chrono>
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
uint64_t to_epoch_ms(const std::chrono::system_clock::time_point& tp);
sercom::protocol::domain::Message to_proto_message(const Message& msg,
                                                   const std::optional<User>& author_opt);
std::string make_message_batch(const ChannelId& ch_id,
                               sercom::protocol::event::MessageBatch::Direction direction,
                               const std::vector<sercom::protocol::domain::Message>& messages);

// app/hub/channel event packaging ---------------------------

std::string make_hub_create(const HubId& hub_id, const std::string& name,
                            const std::string& avatar_seed, const UserId& self_user_id,
                            Role self_role, bool self_online,
                            const std::optional<Channel>& default_channel);
std::string make_hub_already_member(const HubId& hub_id, const UserId& user_id, Role role,
                                    bool is_online);
std::string make_hub_update(const HubId& hub_id, const std::string& name,
                            const std::string& avatar_seed);
std::string make_hub_remove(const HubId& hub_id);
std::string make_member_join(const HubId& hub_id, const UserId& user_id, Role role,
                             const std::string& username, const std::string& avatar_seed,
                             bool is_online);
std::string make_member_leave(const HubId& hub_id, const UserId& user_id);
std::string make_member_presence(const HubId& hub_id, const UserId& user_id, bool is_online);
std::string make_channel_create(const HubId& hub_id, const Channel& channel);
std::string make_channel_update(const HubId& hub_id, const Channel& channel);
std::string make_channel_remove(const HubId& hub_id, const ChannelId& channel_id);

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
