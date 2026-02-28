#pragma once

#include "proto/event/presence.pb.h"

#include <string_view>

namespace app::proto_builders::presence {

inline sercom::protocol::event::PresenceEvent make_presence_changed(std::string_view hub_id,
                                                                    std::string_view user_id,
                                                                    bool is_online) {
    sercom::protocol::event::PresenceEvent presence;
    auto* payload = presence.mutable_presence_changed();
    payload->set_hub_id(hub_id.data(), hub_id.size());
    payload->set_user_id(user_id.data(), user_id.size());
    payload->set_is_online(is_online);
    return presence;
}

inline sercom::protocol::event::PresenceEvent make_typing_started(std::string_view user_id,
                                                                  std::string_view channel_id) {
    sercom::protocol::event::PresenceEvent presence;
    auto* payload = presence.mutable_typing_started();
    payload->set_user_id(user_id.data(), user_id.size());
    payload->set_channel_id(channel_id.data(), channel_id.size());
    return presence;
}

inline sercom::protocol::event::PresenceEvent make_typing_stopped(std::string_view user_id,
                                                                  std::string_view channel_id) {
    sercom::protocol::event::PresenceEvent presence;
    auto* payload = presence.mutable_typing_stopped();
    payload->set_user_id(user_id.data(), user_id.size());
    payload->set_channel_id(channel_id.data(), channel_id.size());
    return presence;
}

}  // namespace app::proto_builders::presence
