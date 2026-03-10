#include "app/services/voice/VoiceSessionManager.h"

namespace app::services::voice {

bool VoiceSessionManager::join(const ChannelId& channel, const UserId& user,
                               const SessionId& session) {
    std::lock_guard lock(mutex_);

    // Preserve mute/deafen state when switching channels.
    ParticipantState carry_state{};
    if (auto it = user_to_channel_.find(user); it != user_to_channel_.end()) {
        auto& old_participants = channels_[it->second.channel];
        if (auto pit = old_participants.find(user); pit != old_participants.end()) {
            carry_state = pit->second;
        }
        old_participants.erase(user);
        if (old_participants.empty()) {
            channels_.erase(it->second.channel);
        }
    }

    auto& participants = channels_[channel];
    const bool was_empty = participants.empty();

    participants.emplace(user, carry_state);
    user_to_channel_[user] = UserVoiceState{channel, session};

    return was_empty;
}

bool VoiceSessionManager::leave(const ChannelId& channel, const UserId& user) {
    std::lock_guard lock(mutex_);

    auto uit = user_to_channel_.find(user);
    if (uit != user_to_channel_.end()) {
        // Ignore stale leave events for a previous channel after the user has moved.
        if (uit->second.channel != channel) {
            return false;
        }
        user_to_channel_.erase(uit);
    }

    auto it = channels_.find(channel);
    if (it == channels_.end()) return false;

    it->second.erase(user);
    const bool became_empty = it->second.empty();
    if (became_empty) channels_.erase(it);

    return became_empty;
}

VoiceSessionManager::LeaveResult VoiceSessionManager::leave_user(const UserId& user) {
    std::lock_guard lock(mutex_);

    auto uit = user_to_channel_.find(user);
    if (uit == user_to_channel_.end()) return {};

    const ChannelId channel = uit->second.channel;
    user_to_channel_.erase(uit);

    auto cit = channels_.find(channel);
    if (cit == channels_.end()) return {channel, true};

    cit->second.erase(user);
    const bool became_empty = cit->second.empty();
    if (became_empty) channels_.erase(cit);

    return {channel, became_empty};
}

VoiceSessionManager::LeaveIfOwnerResult VoiceSessionManager::leave_if_owner(
    const UserId& user, const SessionId& expected_owner_session) {
    std::lock_guard lock(mutex_);

    auto uit = user_to_channel_.find(user);
    if (uit == user_to_channel_.end()) return {};
    if (uit->second.owner_session != expected_owner_session) return {};

    const ChannelId channel = uit->second.channel;
    user_to_channel_.erase(uit);

    auto cit = channels_.find(channel);
    if (cit == channels_.end()) {
        return {.removed = true, .channel = channel, .became_empty = true};
    }

    cit->second.erase(user);
    const bool became_empty = cit->second.empty();
    if (became_empty) channels_.erase(cit);

    return {.removed = true, .channel = channel, .became_empty = became_empty};
}

bool VoiceSessionManager::set_muted(const UserId& user, bool muted) {
    std::lock_guard lock(mutex_);

    auto uit = user_to_channel_.find(user);
    if (uit == user_to_channel_.end()) return false;

    auto cit = channels_.find(uit->second.channel);
    if (cit == channels_.end()) return false;

    auto pit = cit->second.find(user);
    if (pit == cit->second.end()) return false;
    if (pit->second.muted == muted) return false;

    pit->second.muted = muted;
    return true;
}

bool VoiceSessionManager::set_deafened(const UserId& user, bool deafened) {
    std::lock_guard lock(mutex_);

    auto uit = user_to_channel_.find(user);
    if (uit == user_to_channel_.end()) return false;

    auto cit = channels_.find(uit->second.channel);
    if (cit == channels_.end()) return false;

    auto pit = cit->second.find(user);
    if (pit == cit->second.end()) return false;

    bool changed = false;
    if (pit->second.deafened != deafened) {
        pit->second.deafened = deafened;
        changed = true;
    }
    const bool desired_muted = deafened;
    if (pit->second.muted != desired_muted) {
        pit->second.muted = desired_muted;
        changed = true;
    }
    return changed;
}

std::optional<ChannelId> VoiceSessionManager::user_channel(const UserId& user) const {
    std::lock_guard lock(mutex_);
    auto it = user_to_channel_.find(user);
    if (it == user_to_channel_.end()) return std::nullopt;
    return it->second.channel;
}

std::optional<SessionId> VoiceSessionManager::user_session(const UserId& user) const {
    std::lock_guard lock(mutex_);
    auto it = user_to_channel_.find(user);
    if (it == user_to_channel_.end()) return std::nullopt;
    return it->second.owner_session;
}

std::vector<VoiceSessionManager::ParticipantInfo> VoiceSessionManager::participants_in_channel(
    const ChannelId& channel) const {
    std::lock_guard lock(mutex_);
    std::vector<ParticipantInfo> result;

    auto it = channels_.find(channel);
    if (it == channels_.end()) return result;

    result.reserve(it->second.size());
    for (const auto& [user_id, state] : it->second) {
        result.push_back({user_id, state.muted, state.deafened});
    }
    return result;
}

std::vector<UserId> VoiceSessionManager::users_in_channel(const ChannelId& channel) const {
    std::lock_guard lock(mutex_);
    std::vector<UserId> result;

    auto it = channels_.find(channel);
    if (it == channels_.end()) return result;

    result.reserve(it->second.size());
    for (const auto& [user_id, _] : it->second) {
        result.push_back(user_id);
    }
    return result;
}

std::vector<UserId> VoiceSessionManager::clear_channel(const ChannelId& channel) {
    std::lock_guard lock(mutex_);
    std::vector<UserId> removed;

    auto it = channels_.find(channel);
    if (it == channels_.end()) return removed;

    removed.reserve(it->second.size());
    for (const auto& [user_id, _] : it->second) {
        removed.push_back(user_id);
        auto uit = user_to_channel_.find(user_id);
        if (uit != user_to_channel_.end() && uit->second.channel == channel) {
            user_to_channel_.erase(uit);
        }
    }

    channels_.erase(it);
    return removed;
}

bool VoiceSessionManager::is_empty(const ChannelId& channel) const {
    std::lock_guard lock(mutex_);
    auto it = channels_.find(channel);
    return it == channels_.end() || it->second.empty();
}

}  // namespace app::services::voice
