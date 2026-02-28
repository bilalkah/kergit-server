#include "app/services/hub/SnapshotBuilder.h"

namespace app::services {

HubSnapshotBuilder::HubSnapshotBuilder(ChannelService& channel_servise, HubService& service,
                                       PresenceService& presence)
    : channel_servise_(channel_servise), hub_service_(service), presence_(presence) {}

nlohmann::json HubSnapshotBuilder::build(const HubId& hub_id) {
    return {{"type", "hub_snapshot"},
            {"hub_id", hub_id.value},
            {"channels", build_channels(hub_id)},
            {"members", build_members(hub_id)}};
}

nlohmann::json HubSnapshotBuilder::build_channels(const HubId& hub_id) {
    nlohmann::json arr = nlohmann::json::array();
    auto channels = channel_servise_.getHubChannels(hub_id);
    for (const auto& channel : channels) {
        arr.push_back({{"id", channel.id.value},
                       {"hub_id", channel.hub_id.value},
                       {"name", channel.name},
                       {"type", channel.type == ChannelType::VOICE ? "voice" : "text"}});
    }
    return arr;
}

nlohmann::json HubSnapshotBuilder::build_members(const HubId& hub_id) {
    nlohmann::json arr = nlohmann::json::array();

    // Online users from presence
    const auto online_users = presence_.onlineUsersInHub(hub_id);

    std::unordered_map<UserId, bool> online;
    for (const auto& uid : online_users) {
        online[uid] = true;
    }

    const auto members = hub_service_.getHubMembers(hub_id);
    for (const auto& member : members) {
        const auto& user_id = member.user_id;
        std::string display = member.display_name;

        arr.push_back({{"user_id", user_id.value},
                       {"display_name", display},
                       {"online", online.contains(user_id)},
                       {"avatar_seed", member.avatar_seed}});
    }

    return arr;
}

}  // namespace app::services
