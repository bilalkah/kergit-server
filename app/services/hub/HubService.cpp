#include "app/services/hub/HubService.h"

#include "utils/Logger.h"

#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace app::services {

namespace {
std::string role_to_string(Role role) {
    switch (role) {
        case Role::OWNER:
            return "owner";
        case Role::ADMIN:
            return "admin";
        case Role::USER:
            return "member";
        default:
            return "member";
    }
}

ChannelType channel_type_from_storage(const std::string& type) {
    std::string lower = type;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lower == "voice" ? ChannelType::VOICE : ChannelType::CHAT;
}

Hub to_hub(const HubSnapshot& snapshot) {
    Hub hub(snapshot.name, snapshot.hub_id, snapshot.owner_id);
    hub.avatar_seed = snapshot.avatar_seed;
    return hub;
}

}  // namespace

HubService::HubService(HubRepository& repo) : repo_(repo), cache_(std::make_unique<HubCache>()) {}

std::optional<Hub> HubService::getHub(const HubId& hubId) {
    auto cached_snapshot = cache_->get(hubId);
    if (cached_snapshot.has_value()) {
        return to_hub(*cached_snapshot.value());
    }
    if (cached_snapshot.error() == HubCacheError::CorruptedEntry) {
        cache_->invalidate(hubId);
    }
    // Metadata fallback path.
    return repo_.getHub(hubId);
}

std::unordered_map<HubId, Hub> HubService::getHubsByIds(const std::vector<HubId>& hubIds) {
    std::unordered_map<HubId, Hub> hubs_by_id;
    if (hubIds.empty()) {
        return hubs_by_id;
    }

    hubs_by_id.reserve(hubIds.size());
    std::unordered_set<HubId> seen_ids;
    seen_ids.reserve(hubIds.size());
    std::vector<HubId> misses;
    misses.reserve(hubIds.size());

    for (const auto& hub_id : hubIds) {
        if (hub_id.value.empty()) {
            continue;
        }
        if (!seen_ids.insert(hub_id).second) {
            continue;
        }

        auto cached_snapshot = cache_->get(hub_id);
        if (cached_snapshot.has_value()) {
            hubs_by_id.insert_or_assign(hub_id, to_hub(*cached_snapshot.value()));
            continue;
        }

        if (cached_snapshot.error() == HubCacheError::CorruptedEntry) {
            cache_->invalidate(hub_id);
        }
        misses.push_back(hub_id);
    }

    if (misses.empty()) {
        return hubs_by_id;
    }

    const auto fetched_hubs = repo_.getHubsByIds(misses);
    std::vector<HubId> fetched_hub_ids;
    fetched_hub_ids.reserve(fetched_hubs.size());
    for (const auto& hub : fetched_hubs) {
        fetched_hub_ids.push_back(hub.id);
    }
    const auto channels_by_hub = repo_.getHubChannelsByHubIds(fetched_hub_ids);
    const auto member_roles_by_hub = repo_.getHubMemberRolesByHubIds(fetched_hub_ids);
    for (const auto& hub : fetched_hubs) {
        hubs_by_id.insert_or_assign(hub.id, hub);

        HubSnapshot snapshot;
        snapshot.hub_id = hub.id;
        snapshot.owner_id = hub.owner;
        snapshot.name = hub.name;
        snapshot.avatar_seed = hub.avatar_seed;
        if (const auto channels_it = channels_by_hub.find(hub.id);
            channels_it != channels_by_hub.end()) {
            snapshot.channel_ids.reserve(channels_it->second.size());
            for (const auto& channel : channels_it->second) {
                snapshot.channel_ids.push_back(channel.id);
                channel_cache_.put(channel);
            }
        }

        if (const auto members_it = member_roles_by_hub.find(hub.id);
            members_it != member_roles_by_hub.end()) {
            snapshot.member_roles.reserve(members_it->second.size());
            for (const auto& member : members_it->second) {
                snapshot.member_roles.insert_or_assign(member.user_id, member.role);
            }
        }

        cache_->put(std::make_shared<const HubSnapshot>(std::move(snapshot)));
    }

    return hubs_by_id;
}

std::optional<Channel> HubService::getChannel(const ChannelId& channelId) {
    if (const auto cached = channel_cache_.get(channelId); cached.has_value()) {
        return cached;
    }

    const auto channel = repo_.getChannel(channelId);
    if (channel.has_value()) {
        channel_cache_.put(*channel);
    }
    return channel;
}

std::unordered_map<ChannelId, Channel> HubService::getChannelsByIds(
    const std::vector<ChannelId>& channelIds) {
    std::unordered_map<ChannelId, Channel> channels;
    if (channelIds.empty()) {
        return channels;
    }

    channels.reserve(channelIds.size());
    std::unordered_set<ChannelId> seen_ids;
    seen_ids.reserve(channelIds.size());
    std::vector<ChannelId> misses;
    misses.reserve(channelIds.size());

    for (const auto& channel_id : channelIds) {
        if (channel_id.value.empty()) {
            continue;
        }
        if (!seen_ids.insert(channel_id).second) {
            continue;
        }

        if (const auto cached = channel_cache_.get(channel_id); cached.has_value()) {
            channels.insert_or_assign(channel_id, *cached);
            continue;
        }
        misses.push_back(channel_id);
    }

    if (!misses.empty()) {
        const auto fetched_channels = repo_.getChannelsByIds(misses);
        for (const auto& channel : fetched_channels) {
            channel_cache_.put(channel);
            channels.insert_or_assign(channel.id, channel);
        }
    }

    return channels;
}

std::vector<Channel> HubService::getHubChannels(const HubId& hubId) {
    std::vector<Channel> channels;
    const auto channel_ids = getHubChannelIds(hubId);
    if (channel_ids.empty()) {
        return channels;
    }

    const auto by_id = getChannelsByIds(channel_ids);
    channels.reserve(channel_ids.size());
    for (const auto& channel_id : channel_ids) {
        const auto it = by_id.find(channel_id);
        if (it != by_id.end()) {
            channels.push_back(it->second);
        }
    }
    return channels;
}

std::vector<Hub> HubService::getUserHubs(const UserId& userId) {
    // Snapshot is per-hub; there's no in-memory user→hub index to query here.
    return repo_.getUserHubs(userId);
}

void HubService::warmSnapshotsForHubs(const std::vector<Hub>& hubs) {
    if (hubs.empty()) {
        return;
    }

    std::unordered_set<HubId> seen_ids;
    seen_ids.reserve(hubs.size());
    std::unordered_map<HubId, Hub> hubs_by_id;
    hubs_by_id.reserve(hubs.size());
    std::vector<HubId> all_hub_ids;
    all_hub_ids.reserve(hubs.size());
    bool requires_db_refresh = false;

    for (const auto& hub : hubs) {
        if (hub.id.value.empty()) {
            continue;
        }
        if (!seen_ids.insert(hub.id).second) {
            continue;
        }

        all_hub_ids.push_back(hub.id);
        hubs_by_id.insert_or_assign(hub.id, hub);

        auto cached_snapshot = cache_->get(hub.id);
        if (cached_snapshot.has_value()) {
            continue;
        }

        requires_db_refresh = true;
        if (cached_snapshot.error() == HubCacheError::CorruptedEntry) {
            cache_->invalidate(hub.id);
        }
    }

    if (!requires_db_refresh) {
        return;
    }

    // If we touch DB for topology warmup, refresh the whole requested hub set in one pass.
    const auto channels_by_hub = repo_.getHubChannelsByHubIds(all_hub_ids);
    const auto member_roles_by_hub = repo_.getHubMemberRolesByHubIds(all_hub_ids);

    for (const auto& hub_id : all_hub_ids) {
        const auto hub_it = hubs_by_id.find(hub_id);
        if (hub_it == hubs_by_id.end()) {
            continue;
        }

        HubSnapshot snapshot;
        snapshot.hub_id = hub_it->second.id;
        snapshot.owner_id = hub_it->second.owner;
        snapshot.name = hub_it->second.name;
        snapshot.avatar_seed = hub_it->second.avatar_seed;

        if (const auto channels_it = channels_by_hub.find(hub_id);
            channels_it != channels_by_hub.end()) {
            snapshot.channel_ids.reserve(channels_it->second.size());
            for (const auto& channel : channels_it->second) {
                snapshot.channel_ids.push_back(channel.id);
                channel_cache_.put(channel);
            }
        }

        if (const auto members_it = member_roles_by_hub.find(hub_id);
            members_it != member_roles_by_hub.end()) {
            snapshot.member_roles.reserve(members_it->second.size());
            for (const auto& member : members_it->second) {
                snapshot.member_roles.insert_or_assign(member.user_id, member.role);
            }
        }

        cache_->put(std::make_shared<const HubSnapshot>(std::move(snapshot)));
    }
}

HubTopology HubService::getHubTopology(const HubId& hubId) {
    const auto snapshot = loadSnapshot(hubId);
    HubTopology topology;
    if (!snapshot) {
        return topology;
    }

    std::vector<std::pair<UserId, Role>> ordered_members;
    ordered_members.reserve(snapshot->member_roles.size());
    for (const auto& [user_id, role] : snapshot->member_roles) {
        ordered_members.emplace_back(user_id, role);
    }
    std::sort(ordered_members.begin(), ordered_members.end(),
              [](const auto& lhs, const auto& rhs) { return lhs.first.value < rhs.first.value; });

    topology.members.reserve(ordered_members.size());
    for (const auto& [user_id, role] : ordered_members) {
        topology.members.push_back(HubMemberWithRole{user_id, role});
    }

    topology.channel_ids.reserve(snapshot->channel_ids.size());
    for (const auto& channel_id : snapshot->channel_ids) {
        topology.channel_ids.push_back(channel_id);
    }
    return topology;
}

std::vector<HubMemberWithRole> HubService::getHubMembersWithRoles(const HubId& hubId) {
    return getHubTopology(hubId).members;
}

std::vector<ChannelId> HubService::getHubChannelIds(const HubId& hubId) {
    return getHubTopology(hubId).channel_ids;
}

bool HubService::isHubMember(const HubId& hubId, const UserId& userId) {
    return resolveMembershipRole(hubId, userId).has_value();
}

std::optional<Role> HubService::resolveMembershipRole(const HubId& hubId, const UserId& userId) {
    const auto snapshot = loadSnapshot(hubId);
    if (!snapshot) {
        return std::nullopt;
    }

    const auto it = snapshot->member_roles.find(userId);
    if (it != snapshot->member_roles.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<Role> HubService::getMembershipRole(const HubId& hubId, const UserId& userId) {
    return resolveMembershipRole(hubId, userId);
}

Hub HubService::createHub(const std::string& name, const UserId& owner) {
    auto hub = repo_.createHub(name, owner);
    invalidateHubCaches(hub.id);
    return hub;
}

ChannelId HubService::createChannel(const HubId& hubId, const std::string& name,
                                    const std::string& type, const UserId& created_by) {
    const auto channel_id = repo_.createChannel(hubId, name, type, created_by);
    const auto channel_type = channel_type_from_storage(type);
    channel_cache_.put(Channel{name, channel_id, hubId, channel_type});

    auto cached_snapshot = cache_->get(hubId);
    if (cached_snapshot.has_value()) {
        auto snapshot = *cached_snapshot.value();
        if (std::find(snapshot.channel_ids.begin(), snapshot.channel_ids.end(), channel_id) ==
            snapshot.channel_ids.end()) {
            snapshot.channel_ids.push_back(channel_id);
            cache_->put(std::make_shared<const HubSnapshot>(std::move(snapshot)));
        }
    } else if (cached_snapshot.error() == HubCacheError::CorruptedEntry) {
        cache_->invalidate(hubId);
    }
    return channel_id;
}

bool HubService::renameChannel(const ChannelId& channelId, const std::string& newName) {
    const auto renamed = repo_.renameChannel(channelId, newName);
    if (!renamed) {
        return false;
    }

    // A rename only changes the name we already hold, so update the cached Channel in place
    // instead of re-reading it (which could hit the DB on a cache miss). On a miss, drop any
    // stale entry and let the next read repopulate it.
    if (auto cached = channel_cache_.get(channelId); cached.has_value()) {
        cached->name = newName;
        channel_cache_.put(*cached);
    } else {
        channel_cache_.invalidate(channelId);
    }
    return true;
}

bool HubService::deleteChannel(const ChannelId& channelId, const HubId& hubId) {
    const auto deleted = repo_.deleteChannel(channelId, hubId);
    if (!deleted) {
        return false;
    }

    channel_cache_.invalidate(channelId);

    auto cached_snapshot = cache_->get(hubId);
    if (cached_snapshot.has_value()) {
        auto snapshot = *cached_snapshot.value();
        snapshot.channel_ids.erase(
            std::remove(snapshot.channel_ids.begin(), snapshot.channel_ids.end(), channelId),
            snapshot.channel_ids.end());
        cache_->put(std::make_shared<const HubSnapshot>(std::move(snapshot)));
    } else if (cached_snapshot.error() == HubCacheError::CorruptedEntry) {
        cache_->invalidate(hubId);
    }
    return true;
}

bool HubService::renameHub(const HubId& hubId, const std::string& name) {
    auto result = repo_.renameHub(hubId, name);
    if (result) {
        invalidateHubCaches(hubId);
    }
    return result;
}

bool HubService::updateHubAvatarSeed(const HubId& hubId, const std::string& avatar_seed) {
    auto result = repo_.updateHubAvatarSeed(hubId, avatar_seed);
    if (result) {
        invalidateHubCaches(hubId);
    }
    return result;
}

bool HubService::deleteHub(const HubId& hubId, const UserId& owner) {
    std::vector<ChannelId> channel_ids = getHubChannelIds(hubId);
    auto result = repo_.deleteHub(hubId, owner);
    if (result) {
        invalidateHubCaches(hubId);
        for (const auto& channel_id : channel_ids) {
            channel_cache_.invalidate(channel_id);
        }
    }
    return result;
}

void HubService::addMember(const HubId& hubId, const UserId& userId, Role role) {
    repo_.addMember(hubId, userId, role_to_string(role));
    invalidateHubCaches(hubId);
}
void HubService::removeMember(const HubId& hubId, const UserId& userId) {
    repo_.removeMember(hubId, userId);
    invalidateHubCaches(hubId);
}

std::shared_ptr<const HubSnapshot> HubService::buildSnapshot(const HubId& hubId) {
    const auto hub = repo_.getHub(hubId);
    if (!hub.has_value()) {
        return nullptr;
    }

    HubSnapshot snapshot;
    snapshot.hub_id = hub->id;
    snapshot.owner_id = hub->owner;
    snapshot.name = hub->name;
    snapshot.avatar_seed = hub->avatar_seed;

    snapshot.channel_ids = repo_.getHubChannelIds(hubId);

    const auto members = repo_.getHubMemberRoles(hubId);
    snapshot.member_roles.reserve(members.size());
    for (const auto& member : members) {
        snapshot.member_roles.insert_or_assign(member.user_id, member.role);
    }

    auto shared = std::make_shared<const HubSnapshot>(std::move(snapshot));
    cache_->put(shared);
    return shared;
}

std::shared_ptr<const HubSnapshot> HubService::loadSnapshot(const HubId& hubId) {
    auto cached = cache_->get(hubId);
    if (cached.has_value()) {
        return cached.value();
    }
    if (cached.error() == HubCacheError::CorruptedEntry) {
        cache_->invalidate(hubId);
    }
    return buildSnapshot(hubId);
}

void HubService::invalidateSnapshot(const HubId& hubId) { cache_->invalidate(hubId); }

void HubService::invalidateHubCaches(const HubId& hubId) { invalidateSnapshot(hubId); }

}  // namespace app::services
