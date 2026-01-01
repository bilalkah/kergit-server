#include "app/services/hub/HubNotifier.h"

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

nlohmann::json HubNotifier::memberOnline(const HubId& hubId, const UserId& userId) {
    return {
        {"type", "member_online"},
        {"hub_id", ids_.to_public(hubId).value},
        {"user_id", ids_.to_public(userId).value},
    };
}

nlohmann::json HubNotifier::memberOffline(const HubId& hubId, const UserId& userId) {
    return {
        {"type", "member_offline"},
        {"hub_id", ids_.to_public(hubId).value},
        {"user_id", ids_.to_public(userId).value},
    };
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
