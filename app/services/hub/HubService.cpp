#include "app/services/hub/HubService.h"

#include "utils/Logger.h"

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

}  // namespace

HubService::HubService(HubRepository& repo, ChannelRepository& channel_repo)
    : repo_(repo), channel_repo_(channel_repo), cache_(std::make_unique<HubCache>()) {}

std::optional<Hub> HubService::getHub(const HubId& hubId) {
    auto cached = cache_->get(hubId);
    if (cached) {
        return cached.value();
    }
    // HubSnapshot does not include owner_id; DB read is required for full Hub aggregate.
    auto hub = repo_.getHub(hubId);
    if (hub) {
        cache_->put(*hub);
    }
    return hub;
}

std::vector<Hub> HubService::getUserHubs(const UserId& userId) {
    // Snapshot is per-hub; there's no in-memory user→hub index to query here.
    return repo_.getUserHubs(userId);
}

std::vector<HubMemberSummary> HubService::getHubMembers(const HubId& hubId) {
    auto snapshot = getOrBuildSnapshot(hubId);
    std::vector<HubMemberSummary> members;
    members.reserve(snapshot.members.size());
    for (const auto& member : snapshot.members) {
        members.push_back(
            HubMemberSummary{member.user_id, member.display_name, member.avatar_seed});
    }
    return members;
}

bool HubService::isHubMember(const HubId& hubId, const UserId& userId) {
    auto snapshot = getOrBuildSnapshot(hubId);
    for (const auto& member : snapshot.members) {
        if (member.user_id == userId) return true;
    }
    return false;
}
std::optional<Role> HubService::getMembershipRole(const HubId& hubId, const UserId& userId) {
    auto snapshot = getOrBuildSnapshot(hubId);
    for (const auto& member : snapshot.members) {
        if (member.user_id == userId) return member.role;
    }
    return std::nullopt;
}
HubId HubService::createHub(const std::string& name, const UserId& owner) {
    return repo_.createHub(name, owner);
}
bool HubService::renameHub(const HubId& hubId, const std::string& name) {
    auto result = repo_.renameHub(hubId, name);
    if (result) {
        cache_->invalidate(hubId);
        invalidateSnapshot(hubId);
    }
    return result;
}

bool HubService::updateHubAvatarSeed(const HubId& hubId, const std::string& avatar_seed) {
    auto result = repo_.updateHubAvatarSeed(hubId, avatar_seed);
    if (result) {
        cache_->invalidate(hubId);
        invalidateSnapshot(hubId);
    }
    return result;
}

bool HubService::deleteHub(const HubId& hubId, const UserId& owner) {
    auto result = repo_.deleteHub(hubId, owner);
    if (result) {
        cache_->invalidate(hubId);
        invalidateSnapshot(hubId);
    }
    return result;
}

void HubService::addMember(const HubId& hubId, const UserId& userId, Role role) {
    repo_.addMember(hubId, userId, role_to_string(role));
    cache_->invalidate(hubId);
    invalidateSnapshot(hubId);
}
void HubService::removeMember(const HubId& hubId, const UserId& userId) {
    repo_.removeMember(hubId, userId);
    cache_->invalidate(hubId);
    invalidateSnapshot(hubId);
}

HubSnapshot HubService::buildSnapshot(const HubId& hubId) {
    HubSnapshot snapshot;
    snapshot.id = hubId;

    if (auto hub = getHub(hubId)) {
        snapshot.name = hub->name;
        snapshot.avatar_seed = hub->avatar_seed;
    }

    const auto channels = channel_repo_.getHubChannels(hubId);
    snapshot.channels.reserve(channels.size());
    for (const auto& channel : channels) {
        snapshot.channels.push_back(HubSnapshotChannel{channel.id, channel.name, channel.type});
    }

    const auto members = repo_.getHubMembersWithRoles(hubId);
    snapshot.members.reserve(members.size());
    for (const auto& member : members) {
        snapshot.members.push_back(
            HubSnapshotMember{member.user_id, member.display_name, member.avatar_seed, member.role});
    }

    {
        std::unique_lock lock(snapshot_mutex_);
        snapshots_[hubId] = snapshot;
    }

    return snapshot;
}

std::optional<HubSnapshot> HubService::tryGetSnapshot(const HubId& hubId) const {
    std::shared_lock lock(snapshot_mutex_);
    auto it = snapshots_.find(hubId);
    if (it == snapshots_.end()) return std::nullopt;
    return it->second;
}

std::optional<std::pair<HubId, HubSnapshotChannel>> HubService::tryGetSnapshotChannel(
    const ChannelId& channelId) const {
    std::shared_lock lock(snapshot_mutex_);
    for (const auto& [hub_id, snapshot] : snapshots_) {
        for (const auto& channel : snapshot.channels) {
            if (channel.id == channelId) {
                return std::make_pair(hub_id, channel);
            }
        }
    }
    return std::nullopt;
}

HubSnapshot HubService::getOrBuildSnapshot(const HubId& hubId) {
    if (auto cached = tryGetSnapshot(hubId)) {
        // utils::log_line(utils::LogLevel::INFO, "hub_snapshot hit hub_id=" + hubId.value);
        return *cached;
    }
    // utils::log_line(utils::LogLevel::INFO, "hub_snapshot miss hub_id=" + hubId.value + " build=true");
    return buildSnapshot(hubId);
}

void HubService::invalidateSnapshot(const HubId& hubId) {
    std::unique_lock lock(snapshot_mutex_);
    snapshots_.erase(hubId);
}

void HubService::invalidateSnapshotsForChannel(const ChannelId& channelId) {
    std::unique_lock lock(snapshot_mutex_);
    for (auto it = snapshots_.begin(); it != snapshots_.end();) {
        bool matched = false;
        for (const auto& channel : it->second.channels) {
            if (channel.id == channelId) {
                matched = true;
                break;
            }
        }
        if (matched) {
            it = snapshots_.erase(it);
        } else {
            ++it;
        }
    }
}

}  // namespace app::services
