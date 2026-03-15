#include "app/services/hub/HubService.h"

#include "utils/Logger.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <mutex>
#include <sstream>
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

long long elapsed_ms(const std::chrono::steady_clock::time_point start) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                                 start)
        .count();
}

void log_hub_cache(const std::string& op, const std::string& details) {
    (void)op;
    (void)details;
}

}  // namespace

HubService::HubService(HubRepository& repo) : repo_(repo), cache_(std::make_unique<HubCache>()) {}

std::optional<Hub> HubService::getHub(const HubId& hubId) {
    const auto started_at = std::chrono::steady_clock::now();
    auto cached_snapshot = cache_->get(hubId);
    if (cached_snapshot.has_value()) {
        std::ostringstream details;
        details << "hub_id=" << hubId.value << " hit=1 db=0 found=1 total_ms="
                << elapsed_ms(started_at);
        log_hub_cache("getHub", details.str());
        return to_hub(cached_snapshot.value());
    }
    if (cached_snapshot.error() == HubCacheError::CorruptedEntry) {
        cache_->invalidate(hubId);
    }
    // Metadata fallback path.
    const auto db_started_at = std::chrono::steady_clock::now();
    const auto hub = repo_.getHub(hubId);
    const auto db_ms = elapsed_ms(db_started_at);
    std::ostringstream details;
    details << "hub_id=" << hubId.value << " hit=0 db=1 db_ms=" << db_ms
            << " found=" << (hub.has_value() ? 1 : 0)
            << " corrupted_cache_entry=" << (cached_snapshot.error() == HubCacheError::CorruptedEntry)
            << " total_ms=" << elapsed_ms(started_at);
    log_hub_cache("getHub", details.str());
    return hub;
}

std::unordered_map<HubId, Hub> HubService::getHubsByIds(const std::vector<HubId>& hubIds) {
    const auto started_at = std::chrono::steady_clock::now();
    std::unordered_map<HubId, Hub> hubs_by_id;
    if (hubIds.empty()) {
        log_hub_cache("getHubsByIds", "input=0 unique=0 cache_hits=0 db=0 total_ms=0");
        return hubs_by_id;
    }

    hubs_by_id.reserve(hubIds.size());
    std::unordered_set<HubId> seen_ids;
    seen_ids.reserve(hubIds.size());
    std::vector<HubId> misses;
    misses.reserve(hubIds.size());
    size_t cache_hits = 0;
    size_t corrupted_entries = 0;

    for (const auto& hub_id : hubIds) {
        if (hub_id.value.empty()) {
            continue;
        }
        if (!seen_ids.insert(hub_id).second) {
            continue;
        }

        auto cached_snapshot = cache_->get(hub_id);
        if (cached_snapshot.has_value()) {
            hubs_by_id.insert_or_assign(hub_id, to_hub(cached_snapshot.value()));
            ++cache_hits;
            continue;
        }

        if (cached_snapshot.error() == HubCacheError::CorruptedEntry) {
            cache_->invalidate(hub_id);
            ++corrupted_entries;
        }
        misses.push_back(hub_id);
    }

    if (misses.empty()) {
        std::ostringstream details;
        details << "input=" << hubIds.size() << " unique=" << seen_ids.size()
                << " cache_hits=" << cache_hits << " corrupted=" << corrupted_entries
                << " db=0 total_ms=" << elapsed_ms(started_at);
        log_hub_cache("getHubsByIds", details.str());
        return hubs_by_id;
    }

    const auto db_started_at = std::chrono::steady_clock::now();
    const auto fetched_hubs = repo_.getHubsByIds(misses);
    std::vector<HubId> fetched_hub_ids;
    fetched_hub_ids.reserve(fetched_hubs.size());
    for (const auto& hub : fetched_hubs) {
        fetched_hub_ids.push_back(hub.id);
    }
    const auto channels_by_hub = repo_.getHubChannelsByHubIds(fetched_hub_ids);
    const auto member_roles_by_hub = repo_.getHubMemberRolesByHubIds(fetched_hub_ids);
    const auto db_ms = elapsed_ms(db_started_at);
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
                upsertChannelCache(channel);
            }
        }

        if (const auto members_it = member_roles_by_hub.find(hub.id);
            members_it != member_roles_by_hub.end()) {
            snapshot.member_roles.reserve(members_it->second.size());
            for (const auto& member : members_it->second) {
                snapshot.member_roles.insert_or_assign(member.user_id, member.role);
            }
        }

        cache_->put(snapshot);
    }

    std::ostringstream details;
    details << "input=" << hubIds.size() << " unique=" << seen_ids.size()
            << " cache_hits=" << cache_hits << " corrupted=" << corrupted_entries
            << " misses=" << misses.size() << " db=1 db_ms=" << db_ms
            << " fetched=" << fetched_hubs.size() << " total_ms=" << elapsed_ms(started_at);
    log_hub_cache("getHubsByIds", details.str());

    return hubs_by_id;
}

std::optional<Channel> HubService::getChannel(const ChannelId& channelId) {
    const auto started_at = std::chrono::steady_clock::now();
    if (const auto cached = readChannelCache(channelId); cached.has_value()) {
        std::ostringstream details;
        details << "channel_id=" << channelId.value << " hit=1 db=0 found=1 total_ms="
                << elapsed_ms(started_at);
        log_hub_cache("getChannel", details.str());
        return cached;
    }

    const auto db_started_at = std::chrono::steady_clock::now();
    const auto channel = repo_.getChannel(channelId);
    const auto db_ms = elapsed_ms(db_started_at);
    if (channel.has_value()) {
        upsertChannelCache(*channel);
    }

    std::ostringstream details;
    details << "channel_id=" << channelId.value << " hit=0 db=1 db_ms=" << db_ms
            << " found=" << (channel.has_value() ? 1 : 0) << " total_ms=" << elapsed_ms(started_at);
    log_hub_cache("getChannel", details.str());
    return channel;
}

std::unordered_map<ChannelId, Channel> HubService::getChannelsByIds(
    const std::vector<ChannelId>& channelIds) {
    const auto started_at = std::chrono::steady_clock::now();
    std::unordered_map<ChannelId, Channel> channels;
    if (channelIds.empty()) {
        log_hub_cache("getChannelsByIds", "input=0 unique=0 cache_hits=0 db=0 total_ms=0");
        return channels;
    }

    channels.reserve(channelIds.size());
    std::unordered_set<ChannelId> seen_ids;
    seen_ids.reserve(channelIds.size());
    std::vector<ChannelId> misses;
    misses.reserve(channelIds.size());
    size_t cache_hits = 0;

    for (const auto& channel_id : channelIds) {
        if (channel_id.value.empty()) {
            continue;
        }
        if (!seen_ids.insert(channel_id).second) {
            continue;
        }

        if (const auto cached = readChannelCache(channel_id); cached.has_value()) {
            channels.insert_or_assign(channel_id, *cached);
            ++cache_hits;
            continue;
        }
        misses.push_back(channel_id);
    }

    if (!misses.empty()) {
        const auto db_started_at = std::chrono::steady_clock::now();
        const auto fetched_channels = repo_.getChannelsByIds(misses);
        const auto db_ms = elapsed_ms(db_started_at);
        for (const auto& channel : fetched_channels) {
            upsertChannelCache(channel);
            channels.insert_or_assign(channel.id, channel);
        }

        std::ostringstream details;
        details << "input=" << channelIds.size() << " unique=" << seen_ids.size()
                << " cache_hits=" << cache_hits << " misses=" << misses.size() << " db=1 db_ms="
                << db_ms << " fetched=" << fetched_channels.size() << " total_ms="
                << elapsed_ms(started_at);
        log_hub_cache("getChannelsByIds", details.str());
        return channels;
    }

    std::ostringstream details;
    details << "input=" << channelIds.size() << " unique=" << seen_ids.size()
            << " cache_hits=" << cache_hits << " db=0 total_ms=" << elapsed_ms(started_at);
    log_hub_cache("getChannelsByIds", details.str());
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
    const auto started_at = std::chrono::steady_clock::now();
    if (hubs.empty()) {
        log_hub_cache("warmSnapshotsForHubs", "input=0 warmed=0 cache_hits=0 db=0 total_ms=0");
        return;
    }

    std::unordered_set<HubId> seen_ids;
    seen_ids.reserve(hubs.size());
    std::unordered_map<HubId, Hub> hubs_by_id;
    hubs_by_id.reserve(hubs.size());
    std::vector<HubId> all_hub_ids;
    all_hub_ids.reserve(hubs.size());
    size_t cache_hits = 0;
    size_t missing_entries = 0;
    size_t corrupted_entries = 0;
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
            ++cache_hits;
            continue;
        }

        ++missing_entries;
        requires_db_refresh = true;
        if (cached_snapshot.error() == HubCacheError::CorruptedEntry) {
            cache_->invalidate(hub.id);
            ++corrupted_entries;
        }
    }

    if (!requires_db_refresh) {
        std::ostringstream details;
        details << "input=" << hubs.size() << " unique=" << seen_ids.size()
                << " warmed=0 cache_hits=" << cache_hits << " missing=0 corrupted=0 db=0 total_ms="
                << elapsed_ms(started_at);
        log_hub_cache("warmSnapshotsForHubs", details.str());
        return;
    }

    const auto db_started_at = std::chrono::steady_clock::now();
    // If we touch DB for topology warmup, refresh the whole requested hub set in one pass.
    const auto channels_by_hub = repo_.getHubChannelsByHubIds(all_hub_ids);
    const auto member_roles_by_hub = repo_.getHubMemberRolesByHubIds(all_hub_ids);
    const auto db_ms = elapsed_ms(db_started_at);

    size_t warmed = 0;
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
                upsertChannelCache(channel);
            }
        }

        if (const auto members_it = member_roles_by_hub.find(hub_id);
            members_it != member_roles_by_hub.end()) {
            snapshot.member_roles.reserve(members_it->second.size());
            for (const auto& member : members_it->second) {
                snapshot.member_roles.insert_or_assign(member.user_id, member.role);
            }
        }

        cache_->put(snapshot);
        ++warmed;
    }

    std::ostringstream details;
    details << "input=" << hubs.size() << " unique=" << seen_ids.size() << " warmed=" << warmed
            << " cache_hits=" << cache_hits << " missing=" << missing_entries
            << " corrupted=" << corrupted_entries << " refresh_scope=" << all_hub_ids.size()
            << " db=1 db_ms=" << db_ms
            << " total_ms=" << elapsed_ms(started_at);
    log_hub_cache("warmSnapshotsForHubs", details.str());
}

HubTopology HubService::getHubTopology(const HubId& hubId) {
    const auto snapshot = loadSnapshot(hubId);
    HubTopology topology;
    if (!snapshot.has_value()) {
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
    if (!snapshot.has_value()) {
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
                                    const std::string& type) {
    const auto channel_id = repo_.createChannel(hubId, name, type);
    const auto channel_type = channel_type_from_storage(type);
    upsertChannelCache(Channel{name, channel_id, hubId, channel_type});

    auto cached_snapshot = cache_->get(hubId);
    if (cached_snapshot.has_value()) {
        auto snapshot = cached_snapshot.value();
        if (std::find(snapshot.channel_ids.begin(), snapshot.channel_ids.end(), channel_id) ==
            snapshot.channel_ids.end()) {
            snapshot.channel_ids.push_back(channel_id);
            cache_->put(snapshot);
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

    if (auto channel = getChannel(channelId); channel.has_value()) {
        channel->name = newName;
        upsertChannelCache(*channel);
    } else {
        invalidateChannelCache(channelId);
    }
    return true;
}

bool HubService::deleteChannel(const ChannelId& channelId, const HubId& hubId) {
    const auto deleted = repo_.deleteChannel(channelId, hubId);
    if (!deleted) {
        return false;
    }

    invalidateChannelCache(channelId);

    auto cached_snapshot = cache_->get(hubId);
    if (cached_snapshot.has_value()) {
        auto snapshot = cached_snapshot.value();
        snapshot.channel_ids.erase(
            std::remove(snapshot.channel_ids.begin(), snapshot.channel_ids.end(), channelId),
            snapshot.channel_ids.end());
        cache_->put(snapshot);
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
            invalidateChannelCache(channel_id);
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

std::optional<HubSnapshot> HubService::buildSnapshot(const HubId& hubId) {
    const auto started_at = std::chrono::steady_clock::now();
    const auto hub = repo_.getHub(hubId);
    if (!hub.has_value()) {
        std::ostringstream details;
        details << "hub_id=" << hubId.value << " built=0 found=0 total_ms="
                << elapsed_ms(started_at);
        log_hub_cache("buildSnapshot", details.str());
        return std::nullopt;
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

    cache_->put(snapshot);
    std::ostringstream details;
    details << "hub_id=" << hubId.value << " built=1 found=1 channels="
            << snapshot.channel_ids.size() << " members=" << snapshot.member_roles.size()
            << " total_ms=" << elapsed_ms(started_at);
    log_hub_cache("buildSnapshot", details.str());
    return std::optional<HubSnapshot>{snapshot};
}

std::optional<HubSnapshot> HubService::loadSnapshot(const HubId& hubId) {
    const auto started_at = std::chrono::steady_clock::now();
    auto cached = cache_->get(hubId);
    if (cached.has_value()) {
        std::ostringstream details;
        details << "hub_id=" << hubId.value << " hit=1 built=0 total_ms="
                << elapsed_ms(started_at);
        log_hub_cache("loadSnapshot", details.str());
        return std::optional<HubSnapshot>{cached.value()};
    }
    if (cached.error() == HubCacheError::CorruptedEntry) {
        cache_->invalidate(hubId);
    }
    auto built = buildSnapshot(hubId);
    std::ostringstream details;
    details << "hub_id=" << hubId.value << " hit=0 built=" << (built.has_value() ? 1 : 0)
            << " corrupted_cache_entry=" << (cached.error() == HubCacheError::CorruptedEntry)
            << " total_ms=" << elapsed_ms(started_at);
    log_hub_cache("loadSnapshot", details.str());
    return built;
}

std::optional<Channel> HubService::readChannelCache(const ChannelId& channelId) const {
    std::shared_lock lock(channel_cache_mutex_);
    const auto it = channel_cache_.find(channelId);
    if (it == channel_cache_.end()) {
        return std::nullopt;
    }
    return it->second;
}

void HubService::upsertChannelCache(const Channel& channel) {
    std::unique_lock lock(channel_cache_mutex_);
    channel_cache_.insert_or_assign(channel.id, channel);
}

void HubService::invalidateChannelCache(const ChannelId& channelId) {
    std::unique_lock lock(channel_cache_mutex_);
    channel_cache_.erase(channelId);
}

void HubService::invalidateSnapshot(const HubId& hubId) {
    cache_->invalidate(hubId);
}

void HubService::invalidateHubCaches(const HubId& hubId) {
    invalidateSnapshot(hubId);
}

}  // namespace app::services
