#ifndef PROTO_PARSEDPAYLOAD_H
#define PROTO_PARSEDPAYLOAD_H

#include "proto/command/activity.pb.h"
#include "proto/command/channel.pb.h"
#include "proto/command/hub.pb.h"
#include "proto/command/message.pb.h"
#include "proto/command/session.pb.h"
#include "proto/command/user.pb.h"
#include "proto/system/heartbeat.pb.h"

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

}  // namespace sercom::protocol

#endif  // PROTO_PARSEDPAYLOAD_H
