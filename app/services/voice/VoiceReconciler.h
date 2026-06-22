#ifndef APP_SERVICES_VOICE_VOICERECONCILER_H_
#define APP_SERVICES_VOICE_VOICERECONCILER_H_

#include "app/services/voice/ReconcileEvidenceTracker.h"
#include "domains/ids/Ids.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <vector>

namespace app::services::voice {

/**
 * Implemented by the coordinator (VoiceService) to run the actual per-channel reconcile
 * policy. VoiceReconciler only schedules and drives it.
 */
class ReconcileTarget {
   public:
    virtual ~ReconcileTarget() = default;
    virtual void reconcile_channel(const ChannelId& channel, std::string_view reason) = 0;
    virtual void reconcile_full(std::string_view reason) = 0;
};

/**
 * VoiceReconciler
 *
 * Owns the reconcile *mechanism*: the background thread, the on-demand request queue, the
 * periodic interval, and the remote-missing evidence tracker (the grace-period state that
 * decides when a missing room/participant is confirmed gone). It calls back into a
 * ReconcileTarget for the per-channel reconcile policy, which is coordinator work.
 */
class VoiceReconciler {
   public:
    explicit VoiceReconciler(ReconcileTarget& target);
    ~VoiceReconciler();

    void start();
    void stop();

    /// Schedule an on-demand reconcile of a single channel (deduplicated).
    void request(const ChannelId& channel, std::string_view reason);

    /// Evidence accumulation: returns true once the channel/participant has been missing
    /// for the full grace window (caller should then commit cleanup).
    bool confirm_channel_remote_missing(const ChannelId& channel, std::string_view reason);
    void clear_channel_remote_missing_confirmation(const ChannelId& channel);
    bool confirm_participant_remote_missing(const ChannelId& channel, const UserId& user,
                                            std::string_view reason);
    void clear_participant_remote_missing_confirmation(const ChannelId& channel,
                                                       const UserId& user);
    void reset_remote_missing_confirmations(const ChannelId& channel);

    /// Channels currently carrying missing-evidence (folded into the periodic sweep set).
    std::vector<ChannelId> tracked_missing_channels() const;

   private:
    void run_loop();

    ReconcileTarget& target_;
    std::chrono::seconds reconcile_interval_;
    std::chrono::seconds livekit_missing_clear_ttl_;
    ReconcileEvidenceTracker remote_missing_evidence_;

    std::thread reconcile_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::unordered_set<ChannelId> pending_channels_;

    static constexpr std::chrono::seconds kDefaultReconcileInterval{60};
    static constexpr std::chrono::seconds kDefaultLivekitMissingClearTtl{60};
};

}  // namespace app::services::voice

#endif  // APP_SERVICES_VOICE_VOICERECONCILER_H_
