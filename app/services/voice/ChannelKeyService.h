#ifndef APP_SERVICES_VOICE_CHANNELKEYSERVICE_H_
#define APP_SERVICES_VOICE_CHANNELKEYSERVICE_H_

#include "app/services/hub/HubService.h"
#include "app/services/voice/VoicePublisher.h"
#include "app/services/voice/VoiceSessionManager.h"
#include "domains/ids/Ids.h"
#include "infra/redis/RedisClient.h"
#include "livekit/crypto/E2EEKeyManager.h"
#include "livekit/routing/LivekitNodeRegistry.h"
#include "livekit/token/LiveKitTokenService.h"

#include <array>
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
 * ChannelKeyService
 *
 * Owns the full lifecycle of a voice channel's E2EE key: the in-memory key+index store,
 * encrypted at-rest persistence in Redis, per-channel rotation serialization, the
 * forced-rekey path, and broadcasting key updates to participants (via VoicePublisher).
 *
 * Rotation always mints a FRESH random key (never a ratchet) so a departed member cannot
 * derive future keys; the index is only a LiveKit keyring slot for seamless overlap.
 */
class ChannelKeyService {
   public:
    struct AcquireResult {
        // Present on success.
        std::optional<livekit::E2EEKeyManager::ChannelKey> key;
        // Set on failure: "voice_key_unavailable".
        std::string error_reason;
    };

    ChannelKeyService(infra::redis::RedisClient& redis, livekit::LivekitNodeRegistry& nodes,
                      livekit::LiveKitTokenService& token_service, VoiceSessionManager& sessions,
                      VoicePublisher& publisher, app::services::HubService& hub_service);

    /// Resolve the key a joining user should receive: rotates when other members are
    /// present (forward/backward secrecy), else loads/creates the channel key.
    AcquireResult acquire_for_join(const ChannelId& channel, const UserId& user);

    /// Rotate the channel key to fresh random material and broadcast it to participants.
    livekit::E2EEKeyManager::ChannelKey rotate_and_broadcast(const ChannelId& channel,
                                                             std::string_view reason);

    /// Re-send the current key to a user's owner session (resume re-sync).
    void resync_to_user(const UserId& user);

    /// Forget the channel key everywhere (memory + storage).
    void clear_channel(const ChannelId& channel);

    /// Restart recovery: reload an active room's key from storage. When nothing is stored,
    /// the key is left absent (NOT a kick) — clients keep the key they already hold and the
    /// server re-establishes one seamlessly on the next membership change.
    void restore_key_for_recovery(const ChannelId& channel);

   private:
    std::optional<bool> is_channel_effectively_empty(const ChannelId& channel);
    std::mutex& channel_rotation_mutex(const ChannelId& channel);

    // Records that `user` is (re)joining `channel` and reports whether a join-driven
    // rotation should be debounced: true when this same user triggered a join rotation in
    // the last `join_rotate_debounce_ttl_`. Repeated join *requests* that never land in
    // LiveKit (no participant_joined webhook) would otherwise re-rotate the key on every
    // retry, needlessly churning keys for the members already in the room. Always refreshes
    // the user's timestamp so a continuing retry storm stays suppressed.
    bool record_join_and_should_debounce_rotation(const ChannelId& channel, const UserId& user);
    void clear_join_rotation_debounce(const ChannelId& channel);

    static std::string key_storage_key(const ChannelId& channel);
    static std::string index_storage_key(const ChannelId& channel);
    std::optional<livekit::E2EEKeyManager::ChannelKey> load_from_storage(const ChannelId& channel);
    bool persist_to_storage(const ChannelId& channel, std::string_view key, uint32_t key_index);
    void clear_storage(const ChannelId& channel);

    infra::redis::RedisClient& redis_;
    livekit::LivekitNodeRegistry& nodes_;
    livekit::LiveKitTokenService& token_service_;
    VoiceSessionManager& sessions_;
    VoicePublisher& publisher_;
    app::services::HubService& hub_service_;

    livekit::E2EEKeyManager e2ee_keys_;

    std::array<unsigned char, 32> storage_master_key_{};
    bool storage_key_ready_ = false;
    std::chrono::seconds key_ttl_;
    std::chrono::seconds join_rotate_debounce_ttl_;
    // Per-(channel,user) timestamp of the last join-driven rotation, used to debounce
    // rotation on repeated/failed rejoin attempts by the same user.
    std::mutex join_rotate_mutex_;
    std::unordered_map<ChannelId, std::unordered_map<UserId, std::chrono::steady_clock::time_point>>
        join_rotate_seen_;
    // Per-channel serialization of key lifecycle so the assigned index and the
    // VOICE_KEY_UPDATE fanout stay ordered, without one channel blocking another.
    std::mutex rotation_mutex_map_mutex_;
    std::unordered_map<ChannelId, std::unique_ptr<std::mutex>> channel_rotation_mutexes_;

    static constexpr std::chrono::seconds kDefaultKeyTtl{24 * 60 * 60};
    static constexpr std::chrono::seconds kDefaultJoinRotateDebounceTtl{15};
};

}  // namespace app::services::voice

#endif  // APP_SERVICES_VOICE_CHANNELKEYSERVICE_H_
