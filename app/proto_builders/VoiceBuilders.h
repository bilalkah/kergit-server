#pragma once

#include "proto/event/activity.pb.h"

#include <string_view>

namespace app::proto_builders::voice {

inline sercom::protocol::event::VoiceChannelPresence make_voice_presence(
    std::string_view channel_id, std::string_view user_id,
    sercom::protocol::event::VoiceChannelActivityState activity) {
    sercom::protocol::event::VoiceChannelPresence presence;
    presence.set_channel_id(channel_id.data(), channel_id.size());
    presence.set_user_id(user_id.data(), user_id.size());
    presence.set_activity(activity);
    return presence;
}

}  // namespace app::proto_builders::voice
