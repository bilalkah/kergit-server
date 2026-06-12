#ifndef APP_SERVICES_VOICE_RECONCILEEVIDENCETRACKER_H_
#define APP_SERVICES_VOICE_RECONCILEEVIDENCETRACKER_H_

#include "domains/ids/Ids.h"

#include <chrono>
#include <cstddef>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace app::services::voice {

struct ReconcileQuerySummary {
    std::size_t configured_queries = 0;
    std::size_t successful_queries = 0;
    std::size_t failed_queries = 0;
    std::size_t participant_count = 0;

    bool all_queries_succeeded() const {
        return configured_queries > 0 && successful_queries == configured_queries &&
               failed_queries == 0;
    }

    bool unanimously_absent() const {
        return all_queries_succeeded() && participant_count == 0;
    }
};

class ReconcileEvidenceTracker {
   public:
    using Clock = std::chrono::steady_clock;

    struct ObservationResult {
        bool first_observation = false;
        bool confirmed = false;
        std::string first_reason;
    };

    explicit ReconcileEvidenceTracker(std::chrono::seconds missing_ttl);

    ObservationResult observe_channel_missing(const ChannelId& channel, std::string_view reason,
                                              Clock::time_point now = Clock::now());
    ObservationResult observe_participant_missing(const ChannelId& channel, const UserId& user,
                                                  std::string_view reason,
                                                  Clock::time_point now = Clock::now());

    void clear_channel_missing(const ChannelId& channel);
    void clear_participant_missing(const ChannelId& channel, const UserId& user);
    void reset_channel(const ChannelId& channel);
    void reset_all();
    std::vector<ChannelId> tracked_missing_channels() const;

   private:
    struct MissingObservation {
        Clock::time_point first_seen;
        std::string reason;
    };

    std::chrono::seconds missing_ttl_;
    mutable std::mutex mutex_;
    std::unordered_map<ChannelId, MissingObservation> missing_channels_;
    std::unordered_map<ChannelId, std::unordered_map<UserId, MissingObservation>>
        missing_participants_;
};

}  // namespace app::services::voice

#endif  // APP_SERVICES_VOICE_RECONCILEEVIDENCETRACKER_H_
