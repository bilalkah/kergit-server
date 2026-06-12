#include "app/commands/session/StateSyncBuilder.h"

#include "app/commands/utils.h"
#include "domains/Channel.h"
#include "domains/Hub.h"

#include <algorithm>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace app {
namespace {

struct HubSyncInput {
    Hub hub;
    std::vector<app::services::HubMemberWithRole> members;
    std::vector<ChannelId> channel_ids;
};

void populate_self_user(CommandContext& ctx, const UserId& user_id,
                        sercom::protocol::event::StateSync& sync) {
    if (const auto db_user = ctx.user_service.getUser(user_id)) {
        *sync.mutable_self() = to_proto_user(*db_user);
    } else {
        sync.mutable_self()->set_id(user_id.value);
    }
}

std::vector<HubSyncInput> collect_hub_inputs(CommandContext& ctx, const std::vector<Hub>& hubs) {
    std::vector<HubSyncInput> hub_inputs;
    hub_inputs.reserve(hubs.size());
    if (hubs.empty()) {
        return hub_inputs;
    }

    ctx.hub_service.warmSnapshotsForHubs(hubs);

    for (const auto& hub : hubs) {
        HubSyncInput input;
        input.hub = hub;
        auto topology = ctx.hub_service.getHubTopology(hub.id);
        input.members = std::move(topology.members);
        input.channel_ids = std::move(topology.channel_ids);
        hub_inputs.push_back(std::move(input));
    }
    return hub_inputs;
}

std::vector<UserId> gather_user_ids(const std::vector<HubSyncInput>& hub_inputs) {
    std::vector<UserId> user_ids;
    std::unordered_set<UserId> seen_ids;
    for (const auto& input : hub_inputs) {
        for (const auto& member : input.members) {
            if (member.user_id.value.empty()) {
                continue;
            }
            if (!seen_ids.insert(member.user_id).second) {
                continue;
            }
            user_ids.push_back(member.user_id);
        }
    }
    return user_ids;
}

std::vector<ChannelId> gather_channel_ids(const std::vector<HubSyncInput>& hub_inputs) {
    std::vector<ChannelId> channel_ids;
    std::unordered_set<ChannelId> seen_ids;
    for (const auto& input : hub_inputs) {
        for (const auto& channel_id : input.channel_ids) {
            if (channel_id.value.empty()) {
                continue;
            }
            if (!seen_ids.insert(channel_id).second) {
                continue;
            }
            channel_ids.push_back(channel_id);
        }
    }
    return channel_ids;
}

void append_hub_sync(CommandContext& ctx, const HubSyncInput& input,
                     const std::unordered_map<UserId, User>& users_by_id,
                     const std::unordered_map<ChannelId, Channel>& channels_by_id,
                     sercom::protocol::event::StateSync& sync) {
    auto* hub_sync = sync.add_hubs();
    hub_sync->mutable_hub()->set_id(input.hub.id.value);
    hub_sync->mutable_hub()->set_name(input.hub.name);
    hub_sync->mutable_hub()->mutable_metadata()->set_avatar_seed(input.hub.avatar_seed);

    for (const auto& member : input.members) {
        const auto is_online = ctx.presence_manager.isUserOnline(member.user_id);
        auto* member_state = hub_sync->add_members();
        *member_state->mutable_member() =
            to_proto_hub_member(member.user_id, std::optional<Role>{member.role}, is_online);

        auto* user_state = hub_sync->add_users();
        auto* user = user_state->mutable_user();
        auto user_it = users_by_id.find(member.user_id);
        if (user_it != users_by_id.end()) {
            *user = to_proto_user(user_it->second);
        } else {
            user->set_id(member.user_id.value);
        }
    }

    for (const auto& channel_id : input.channel_ids) {
        auto channel_it = channels_by_id.find(channel_id);
        if (channel_it == channels_by_id.end()) {
            auto* channel_state = hub_sync->add_channels();
            channel_state->mutable_channel()->set_id(channel_id.value);
            continue;
        }
        const auto& channel = channel_it->second;

        auto* channel_state = hub_sync->add_channels();
        *channel_state->mutable_channel() = to_proto_channel(channel);
        if (channel.type != ChannelType::VOICE) {
            continue;
        }

        auto participants = ctx.voice_service.sessions().participants_in_channel(channel.id);
        auto* voice = channel_state->mutable_voice();
        voice->set_started_at_unix(ctx.voice_service.channel_started_at_unix(channel.id));
        for (const auto& participant : participants) {
            auto* out = voice->add_participants();
            out->set_user_id(participant.user_id.value);
            out->set_muted(participant.muted);
            out->set_deafened(participant.deafened);
        }
    }
}

}  // namespace

sercom::protocol::event::StateSync build_state_sync_for_user(CommandContext& ctx,
                                                             const UserId& user_id) {
    sercom::protocol::event::StateSync sync;
    populate_self_user(ctx, user_id, sync);

    const auto hubs = ctx.hub_service.getUserHubs(user_id);
    const auto hub_inputs = collect_hub_inputs(ctx, hubs);
    const auto users_by_id = ctx.user_service.getUsersByIds(gather_user_ids(hub_inputs));
    const auto channels_by_id = ctx.hub_service.getChannelsByIds(gather_channel_ids(hub_inputs));
    for (const auto& input : hub_inputs) {
        append_hub_sync(ctx, input, users_by_id, channels_by_id, sync);
    }
    return sync;
}

sercom::protocol::event::StateSync build_state_sync_for_requested_hubs(
    CommandContext& ctx, const UserId& user_id,
    const std::unordered_set<HubId>& requested_hub_ids) {
    sercom::protocol::event::StateSync sync;
    populate_self_user(ctx, user_id, sync);

    if (requested_hub_ids.empty()) {
        return sync;
    }

    std::vector<HubId> requested_ids;
    requested_ids.reserve(requested_hub_ids.size());
    for (const auto& hub_id : requested_hub_ids) {
        requested_ids.push_back(hub_id);
    }
    std::sort(requested_ids.begin(), requested_ids.end(),
              [](const HubId& a, const HubId& b) { return a.value < b.value; });

    std::vector<HubId> authorized_hub_ids;
    authorized_hub_ids.reserve(requested_ids.size());
    for (const auto& hub_id : requested_ids) {
        if (ctx.hub_service.getMembershipRole(hub_id, user_id).has_value()) {
            authorized_hub_ids.push_back(hub_id);
        }
    }

    const auto hubs_by_id = ctx.hub_service.getHubsByIds(authorized_hub_ids);
    std::vector<Hub> ordered_hubs;
    ordered_hubs.reserve(authorized_hub_ids.size());
    for (const auto& hub_id : authorized_hub_ids) {
        const auto it = hubs_by_id.find(hub_id);
        if (it == hubs_by_id.end()) {
            continue;
        }
        ordered_hubs.push_back(it->second);
    }

    const auto hub_inputs = collect_hub_inputs(ctx, ordered_hubs);
    const auto users_by_id = ctx.user_service.getUsersByIds(gather_user_ids(hub_inputs));
    const auto channels_by_id = ctx.hub_service.getChannelsByIds(gather_channel_ids(hub_inputs));
    for (const auto& input : hub_inputs) {
        append_hub_sync(ctx, input, users_by_id, channels_by_id, sync);
    }

    return sync;
}

}  // namespace app
