#ifndef APP_VOICE_VOICESERVICE_H_
#define APP_VOICE_VOICESERVICE_H_

#include "app/managers/session/SessionManager.h"
#include "app/managers/subscription/SubscriptionManager.h"
#include "app/services/hub/HubService.h"
#include "app/services/voice/ChannelKeyService.h"
#include "app/services/voice/PendingJoinIntentStore.h"
#include "app/services/voice/VoicePublisher.h"
#include "app/services/voice/VoiceReconciler.h"
#include "app/services/voice/VoiceResumeRegistry.h"
#include "app/services/voice/VoiceSessionManager.h"
#include "core/LivekitClusterConfig.h"
#include "domains/ids/Ids.h"
#include "infra/redis/RedisClient.h"
#include "livekit/cli/LivekitClient.h"
#include "livekit/routing/LivekitNodeRegistry.h"
#include "livekit/token/LiveKitTokenService.h"
#include "livekit/webhook/LivekitEvent.h"
#include "net/outbound/IOutBoundSink.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace app::services::voice {

/**
 * VoiceService
 *
 * Command path responsibilities:
 *  - validate/takeover control helpers (verified kick)
 *  - issue tokens with app-session metadata
 *  - stage Redis pending intents
 *
 * Webhook path responsibilities (authoritative truth):
 *  - dedup + intent/session correlation
 *  - mutate VoiceSessionManager
 *  - persist DB state
 *  - fanout participants/activity/self-status
 */
class VoiceService : public ReconcileTarget {
   public:
    struct JoinVoiceToken {
        std::string token;
        std::string livekit_url;
        uint64_t expires_in = 0;
        std::string e2ee_key;
        uint32_t key_index = 0;
        std::string resume_id;
        std::string error_reason;
    };

    struct ResumeTransferResult {
        SessionId previous_owner_session = 0;
        ChannelId channel;
        std::string resume_id;
    };

    // PendingJoinIntent now lives in PendingJoinIntentStore; alias kept so existing
    // callers (and this class) can keep using VoiceService::PendingJoinIntent.
    using PendingJoinIntent = ::app::services::voice::PendingJoinIntent;

    VoiceService(std::string api_key, std::string api_secret, infra::redis::RedisClient& redis,
                 SessionManager& session_manager, SubscriptionManager& subscription_manager,
                 app::services::HubService& hub_service,
                 net::outbound::IOutboundSink& outbound_sink,
                 const std::vector<core::LivekitNodeConfig>& nodes,
                 std::string livekit_cluster_url);
    ~VoiceService();

    /// Start/stop periodic LiveKit state reconciliation loop.
    void start_reconcile_loop();
    void stop_reconcile_loop();

    /// Issue a LiveKit token + E2EE key for a user joining a voice channel.
    JoinVoiceToken join_voice(const ChannelId& channel, const UserId& user,
                              SessionId app_session_id, std::string_view intent_nonce);

    /// Stage pending join/switch intent for webhook correlation.
    bool stage_pending_join_intent(const UserId& user, const PendingJoinIntent& intent,
                                   uint64_t expires_in_seconds);
    void clear_pending_join_intent(const UserId& user);

    /// Admin action: remove-authoritative kick with fallback participant diagnostics.
    bool verified_kick_user(const ChannelId& channel, const UserId& target);

    /// Admin action: best-effort kick.
    bool kick_user(const ChannelId& channel, const UserId& target);

    /// Called when the last participant leaves a channel.
    /// Only clears E2EE material.
    void on_channel_empty(const ChannelId& channel);

    /// Final room/node/channel cleanup path.
    void on_channel_finish(const ChannelId& channel);

    /// Called for each incoming LiveKit webhook event.
    void on_livekit_event(const livekit::webhook::LiveKitEvent& event);

    /// Called once during bootstrap to recover voice state after a server restart.
    void recover_from_restart();

    /// Mark that ROOM_FINISHED for this channel should be ignored during takeover race windows.
    void mark_channel_takeover(const ChannelId& channel);

    /// Returns voice channel start unix time (seconds). 0 means inactive/unknown.
    uint64_t channel_started_at_unix(const ChannelId& channel) const;

    /// Access voice session state.
    VoiceSessionManager& sessions() { return sessions_; }
    const VoiceSessionManager& sessions() const { return sessions_; }
    std::uint64_t active_voice_user_count() const;

    /// Utility for command-path nonce generation.
    std::string generate_intent_nonce() const;

    /// Called when a user's last active session is destroyed (all WebSocket connections closed).
    /// Schedules an immediate reconcile if the user is a voice participant, allowing the
    /// reconcile loop to detect a missed LiveKit webhook and clean up ghost presence.
    void on_session_destroyed(const UserId& user);

    /// Returns current one-time resume id for the user's active voice ownership, if any.
    std::optional<std::string> current_resume_id(const UserId& user) const;

    /// Attempts reconnect-safe ownership transfer using a one-time resume id.
    /// On success, ownership is moved to next_owner_session and resume id is rotated.
    std::optional<ResumeTransferResult> try_resume_voice_ownership(const UserId& user,
                                                                   const HubId& hub,
                                                                   const ChannelId& channel,
                                                                   std::string_view resume_id,
                                                                   SessionId next_owner_session);

    /// Re-send the current channel E2EE key+index to a user's owner session. Used after
    /// a resume reconnect so a client that missed a key rotation during its reconnect
    /// window re-syncs to the live key.
    void resync_voice_key_for_user(const UserId& user);

   private:
    bool consume_channel_takeover_guard(const ChannelId& channel);
    bool mark_webhook_event_seen(const std::string& event_id);
    bool is_stale_participant_event(const livekit::webhook::LiveKitEvent& event);
    bool is_stale_room_event(const livekit::webhook::LiveKitEvent& event);

    void handle_participant_joined(const livekit::webhook::LiveKitEvent& event);
    void handle_participant_left(const livekit::webhook::LiveKitEvent& event);
    void force_local_leave(const UserId& user, const ChannelId& channel, std::string_view reason);

    struct ParticipantMetadata {
        std::string node_id;
        uint64_t app_session_id = 0;
        std::string intent_nonce;
        bool structured = false;
    };
    static ParticipantMetadata parse_participant_metadata(std::string_view metadata);
    std::optional<std::string> resolve_participant_node_assignment(
        const ChannelId& channel,
        const std::unordered_map<UserId, livekit::cli::ParticipantInfo>& participants,
        std::string_view reason) const;

    // ReconcileTarget — invoked by VoiceReconciler on its schedule.
    void reconcile_channel(const ChannelId& channel, std::string_view reason) override;
    void reconcile_full(std::string_view reason) override;
    void reconcile_full_state(std::string_view reason);
    void reconcile_channel_state(const ChannelId& channel, std::string_view reason);
    bool has_active_owner_connection(const UserId& user, const ChannelId& channel) const;
    std::size_t active_owner_connection_count(const ChannelId& channel) const;

    void set_channel_started_at_unix(const ChannelId& channel, uint64_t started_at_unix);
    void clear_channel_started_at_unix(const ChannelId& channel);
    uint64_t read_channel_started_at_unix(const ChannelId& channel) const;

    static std::string webhook_seen_key(const std::string& event_id);

    infra::redis::RedisClient& redis_;
    SessionManager& session_manager_;
    SubscriptionManager& subscription_manager_;
    app::services::HubService& hub_service_;
    net::outbound::IOutboundSink& outbound_sink_;

    livekit::LiveKitTokenService token_service_;
    livekit::LivekitNodeRegistry nodes_;
    // Single client-facing LiveKit endpoint (Caddy load-balances it across all nodes).
    std::string livekit_cluster_url_;
    VoiceSessionManager sessions_;
    PendingJoinIntentStore pending_intents_;
    VoiceResumeRegistry resume_registry_;
    VoicePublisher publisher_;
    ChannelKeyService channel_keys_;

    std::mutex takeover_guard_mutex_;
    std::unordered_map<ChannelId, std::chrono::steady_clock::time_point> channel_takeover_guard_;
    mutable std::mutex channel_started_mutex_;
    std::unordered_map<ChannelId, uint64_t> channel_started_at_unix_;
    mutable std::mutex event_order_mutex_;
    std::unordered_map<UserId, uint64_t> user_last_event_ts_ms_;
    std::unordered_map<ChannelId, uint64_t> channel_last_room_event_ts_ms_;

    // Declared last so its destructor (which joins the reconcile thread) runs before the
    // members the thread's callbacks touch.
    VoiceReconciler reconciler_;

    static constexpr std::chrono::seconds kTakeoverGuardTtl{45};
    static constexpr std::chrono::seconds kWebhookDedupTtl{24 * 60 * 60};
};

}  // namespace app::services::voice

#endif  // APP_VOICE_VOICESERVICE_H_
