#include "app/services/voice/ReconcileEvidenceTracker.h"

namespace app::services::voice {

ReconcileEvidenceTracker::ReconcileEvidenceTracker(std::chrono::seconds missing_ttl)
    : missing_ttl_(missing_ttl) {}

ReconcileEvidenceTracker::ObservationResult ReconcileEvidenceTracker::observe_channel_missing(
    const ChannelId& channel, std::string_view reason, Clock::time_point now) {
    std::lock_guard lock(mutex_);
    auto [it, inserted] =
        missing_channels_.try_emplace(channel, MissingObservation{now, std::string(reason)});

    ObservationResult result{.first_observation = inserted,
                             .confirmed = !inserted && now - it->second.first_seen >= missing_ttl_,
                             .first_reason = it->second.reason};
    if (result.confirmed) {
        missing_channels_.erase(it);
        missing_participants_.erase(channel);
    }
    return result;
}

ReconcileEvidenceTracker::ObservationResult ReconcileEvidenceTracker::observe_participant_missing(
    const ChannelId& channel, const UserId& user, std::string_view reason, Clock::time_point now) {
    std::lock_guard lock(mutex_);
    auto& by_user = missing_participants_[channel];
    auto [it, inserted] = by_user.try_emplace(user, MissingObservation{now, std::string(reason)});

    ObservationResult result{.first_observation = inserted,
                             .confirmed = !inserted && now - it->second.first_seen >= missing_ttl_,
                             .first_reason = it->second.reason};
    if (result.confirmed) {
        by_user.erase(it);
        if (by_user.empty()) {
            missing_participants_.erase(channel);
        }
    }
    return result;
}

void ReconcileEvidenceTracker::clear_channel_missing(const ChannelId& channel) {
    std::lock_guard lock(mutex_);
    missing_channels_.erase(channel);
}

void ReconcileEvidenceTracker::clear_participant_missing(const ChannelId& channel,
                                                         const UserId& user) {
    std::lock_guard lock(mutex_);
    auto channel_it = missing_participants_.find(channel);
    if (channel_it == missing_participants_.end()) return;
    channel_it->second.erase(user);
    if (channel_it->second.empty()) {
        missing_participants_.erase(channel_it);
    }
}

void ReconcileEvidenceTracker::reset_channel(const ChannelId& channel) {
    std::lock_guard lock(mutex_);
    missing_channels_.erase(channel);
    missing_participants_.erase(channel);
}

void ReconcileEvidenceTracker::reset_all() {
    std::lock_guard lock(mutex_);
    missing_channels_.clear();
    missing_participants_.clear();
}

std::vector<ChannelId> ReconcileEvidenceTracker::tracked_missing_channels() const {
    std::lock_guard lock(mutex_);
    std::vector<ChannelId> channels;
    channels.reserve(missing_channels_.size());
    for (const auto& [channel, _] : missing_channels_) {
        channels.push_back(channel);
    }
    return channels;
}

}  // namespace app::services::voice
