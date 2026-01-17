#include "app/services/hub/HubNotifier.h"

#include "proto/envelope.pb.h"
#include "proto/event/presence.pb.h"

namespace app::services {
HubNotifier::HubNotifier(PublicIdService& ids) : ids_(ids) {}

nlohmann::json HubNotifier::hubRenamed(const HubId& hubId, const std::string& newName) {
    return {
        {"type", "hub_renamed"},
        {"hub_id", ids_.to_public(hubId).value},
        {"name", newName},
    };
}

nlohmann::json HubNotifier::hubDeleted(const HubId& hubId) {
    return {
        {"type", "hub_deleted"},
        {"hub_id", ids_.to_public(hubId).value},
    };
}

nlohmann::json HubNotifier::memberJoined(const HubId& hubId, const UserId& userId) {
    return {
        {"type", "member_joined"},
        {"hub_id", ids_.to_public(hubId).value},
        {"user_id", ids_.to_public(userId).value},
    };
}

nlohmann::json HubNotifier::memberLeft(const HubId& hubId, const UserId& userId) {
    return {
        {"type", "member_left"},
        {"hub_id", ids_.to_public(hubId).value},
        {"user_id", ids_.to_public(userId).value},
    };
}

std::string HubNotifier::memberOnline(const HubId& hubId, const UserId& userId) {
    sercom::protocol::event::PresenceEvent pe;
    auto* pc = pe.mutable_presence_changed();
    pc->set_hub_id(ids_.to_public(hubId).value);
    pc->set_user_id(ids_.to_public(userId).value);
    pc->set_is_online(true);
    std::string pe_payload;
    pe.SerializeToString(&pe_payload);
    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(sercom::protocol::Envelope::PRESENCE);
    env.set_payload(std::move(pe_payload));
    std::string env_serialized;
    env.SerializeToString(&env_serialized);
    return env_serialized;
}

std::string HubNotifier::memberOffline(const HubId& hubId, const UserId& userId) {
    sercom::protocol::event::PresenceEvent pe;
    auto* pc = pe.mutable_presence_changed();
    pc->set_hub_id(ids_.to_public(hubId).value);
    pc->set_user_id(ids_.to_public(userId).value);
    pc->set_is_online(false);
    std::string pe_payload;
    pe.SerializeToString(&pe_payload);
    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(sercom::protocol::Envelope::PRESENCE);
    env.set_payload(std::move(pe_payload));
    std::string env_serialized;
    env.SerializeToString(&env_serialized);
    return env_serialized;
}

nlohmann::json HubNotifier::channelCreated(const HubId& hubId, const ChannelId& channelId) {
    return {
        {"type", "channel_created"},
        {"hub_id", ids_.to_public(hubId).value},
        {"channel_id", ids_.to_public(channelId).value},
    };
}

nlohmann::json HubNotifier::channelRenamed(const HubId& hubId, const ChannelId& channelId,
                                           const std::string& name) {
    return {
        {"type", "channel_renamed"},
        {"hub_id", ids_.to_public(hubId).value},
        {"channel_id", ids_.to_public(channelId).value},
        {"name", name},
    };
}

nlohmann::json HubNotifier::channelDeleted(const HubId& hubId, const ChannelId& channelId) {
    return {
        {"type", "channel_deleted"},
        {"hub_id", ids_.to_public(hubId).value},
        {"channel_id", ids_.to_public(channelId).value},
    };
}
}  // namespace app::services
