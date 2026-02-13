#pragma once

#include "proto/event/presence.pb.h"

#include <cstdint>

namespace app::proto_builders::presence {

inline sercom::protocol::event::PresenceEvent make_presence_changed(uint64_t hub_id,
                                                                    uint64_t user_id,
                                                                    bool is_online) {
    sercom::protocol::event::PresenceEvent presence;
    auto* payload = presence.mutable_presence_changed();
    payload->set_hub_id(hub_id);
    payload->set_user_id(user_id);
    payload->set_is_online(is_online);
    return presence;
}

inline sercom::protocol::event::PresenceEvent make_typing_started(uint64_t hub_id, uint64_t user_id,
                                                                  uint64_t channel_id) {
    sercom::protocol::event::PresenceEvent presence;
    auto* payload = presence.mutable_typing_started();
    payload->set_hub_id(hub_id);
    payload->set_user_id(user_id);
    payload->set_channel_id(channel_id);
    return presence;
}

inline sercom::protocol::event::PresenceEvent make_typing_stopped(uint64_t hub_id, uint64_t user_id,
                                                                  uint64_t channel_id) {
    sercom::protocol::event::PresenceEvent presence;
    auto* payload = presence.mutable_typing_stopped();
    payload->set_hub_id(hub_id);
    payload->set_user_id(user_id);
    payload->set_channel_id(channel_id);
    return presence;
}

}  // namespace app::proto_builders::presence
