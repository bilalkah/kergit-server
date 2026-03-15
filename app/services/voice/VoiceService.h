#ifndef APP_VOICE_VOICESERVICE_H_
#define APP_VOICE_VOICESERVICE_H_

#include "app/managers/session/SessionManager.h"
#include "app/managers/subscription/SubscriptionManager.h"
#include "app/services/hub/HubService.h"
#include "app/services/voice/VoiceSessionManager.h"
#include "domains/ids/Ids.h"
#include "infra/persistence/repositories/VoiceStateRepository.h"
#include "infra/redis/RedisClient.h"
#include "livekit/crypto/E2EEKeyManager.h"
#include "livekit/routing/LivekitNodeRegistry.h"
#include "livekit/token/LiveKitTokenService.h"
#include "livekit/webhook/LivekitEvent.h"
#include "net/outbound/IOutBoundSink.h"

#include <chrono>
#include <cstdint>
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
class VoiceService {
   public:
    struct JoinVoiceToken {
        std::string token;
        std::string livekit_url;
        uint64_t expires_in = 0;
        std::string e2ee_key;
    };

    struct PendingJoinIntent {
        SessionId session_id = 0;
        std::string intent_nonce;

        ChannelId to_channel;
        ChannelId from_channel;
        bool has_from_channel = false;

        bool muted = false;
        bool deafened = false;

        bool old_leave_seen = false;
        bool new_join_seen = false;

        uint64_t expires_at_unix = 0;
    };

    VoiceService(std::string api_key, std::string api_secret,
                 VoiceStateRepository& voice_state_repo, infra::redis::RedisClient& redis,
                 SessionManager& session_manager, SubscriptionManager& subscription_manager,
                 app::services::HubService& hub_service,
                 net::outbound::IOutboundSink& outbound_sink);

    /// Issue a LiveKit token + E2EE key for a user joining a voice channel.
    JoinVoiceToken join_voice(const ChannelId& channel, const UserId& user,
                              SessionId app_session_id,
                              std::string_view intent_nonce);

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

    /// Persist voice join upsert.
    void persist_voice_join(const UserId& user, const ChannelId& channel, bool muted,
                            bool deafened);

    /// Persist voice leave.
    void persist_voice_leave(const UserId& user);

    /// Returns voice channel start unix time (seconds). 0 means inactive/unknown.
    uint64_t channel_started_at_unix(const ChannelId& channel) const;

    /// Access voice session state.
    VoiceSessionManager& sessions() { return sessions_; }
    const VoiceSessionManager& sessions() const { return sessions_; }

    /// Utility for command-path nonce generation.
    std::string generate_intent_nonce() const;

   private:
    bool consume_channel_takeover_guard(const ChannelId& channel);
    bool mark_webhook_event_seen(const std::string& event_id);

    std::optional<PendingJoinIntent> read_pending_join_intent(const UserId& user) const;
    bool update_pending_join_intent(const UserId& user, const PendingJoinIntent& intent);

    void handle_participant_joined(const livekit::webhook::LiveKitEvent& event);
    void handle_participant_left(const livekit::webhook::LiveKitEvent& event);

    void publish_voice_snapshot(const HubId& hub, const ChannelId& channel, uint64_t started_at_unix);
    void publish_voice_participant_upsert(const HubId& hub, const ChannelId& channel,
                                          const UserId& user, bool muted, bool deafened);
    void publish_voice_participant_remove(const HubId& hub, const ChannelId& channel,
                                          const UserId& user);
    void publish_self_status(const UserId& user, bool connected,
                             const std::optional<SessionId>& owner_session_id,
                             const std::optional<ChannelId>& channel,
                             std::optional<SessionId> only_session_id = std::nullopt);

    void set_channel_started_at_unix(const ChannelId& channel, uint64_t started_at_unix);
    void clear_channel_started_at_unix(const ChannelId& channel);
    uint64_t read_channel_started_at_unix(const ChannelId& channel) const;

    // Webhook-only fanout. Guarded at runtime; calls outside LiveKit event dispatch are rejected.
    void emit(net::outbound::OutgoingMessage msg);

    static std::string pending_join_key(const UserId& user);
    static std::string webhook_seen_key(const std::string& event_id);

    VoiceStateRepository& voice_state_repo_;
    infra::redis::RedisClient& redis_;
    SessionManager& session_manager_;
    SubscriptionManager& subscription_manager_;
    app::services::HubService& hub_service_;
    net::outbound::IOutboundSink& outbound_sink_;

    livekit::LiveKitTokenService token_service_;
    livekit::LivekitNodeRegistry nodes_;
    livekit::E2EEKeyManager e2ee_keys_;
    VoiceSessionManager sessions_;

    std::mutex takeover_guard_mutex_;
    std::unordered_map<ChannelId, std::chrono::steady_clock::time_point> channel_takeover_guard_;
    mutable std::mutex channel_started_mutex_;
    std::unordered_map<ChannelId, uint64_t> channel_started_at_unix_;

    static constexpr std::chrono::seconds kTakeoverGuardTtl{45};
    static constexpr std::chrono::seconds kWebhookDedupTtl{24 * 60 * 60};
};

}  // namespace app::services::voice

#endif  // APP_VOICE_VOICESERVICE_H_
