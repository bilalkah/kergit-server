#pragma once

#include "proto/event/activity.pb.h"

#include <cstdint>

namespace app::proto_builders::voice {

inline sercom::protocol::event::VoiceChannelPresence make_voice_presence(
    uint64_t hub_id, uint64_t channel_id, uint64_t user_id,
    sercom::protocol::event::VoiceChannelPresence_State state) {
    sercom::protocol::event::VoiceChannelPresence presence;
    presence.set_hub_id(hub_id);
    presence.set_channel_id(channel_id);
    presence.set_state(state);
    presence.set_user_id(user_id);
    return presence;
}

}  // namespace app::proto_builders::voice
