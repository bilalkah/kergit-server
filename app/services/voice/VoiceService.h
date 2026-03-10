#ifndef APP_VOICE_VOICESERVICE_H_
#define APP_VOICE_VOICESERVICE_H_

#include "app/services/voice/VoiceSessionManager.h"
#include "domains/ids/Ids.h"
#include "infra/persistence/repositories/VoiceStateRepository.h"
#include "livekit/crypto/E2EEKeyManager.h"
#include "livekit/routing/LivekitNodeRegistry.h"
#include "livekit/token/LiveKitTokenService.h"
#include "livekit/webhook/LivekitEvent.h"
#include "proto/event/activity.pb.h"

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>

namespace app::services::voice {

/**
 * VoiceService
 *
 * Owns all LiveKit infrastructure (node registry, JWT tokens, E2EE keys)
 * and voice session state (VoiceSessionManager).
 *
 * Commands call join_voice / leave_voice.
 * Webhooks call on_livekit_event which updates VoiceSessionManager.
 */
class VoiceService {
   public:
    VoiceService(std::string api_key, std::string api_secret,
                 VoiceStateRepository& voice_state_repo);

    /// Issue a LiveKit token + E2EE key for a user joining a voice channel.
    /// Returns a fully populated VoiceTokenIssued message (url, token, key).
    sercom::protocol::event::VoiceTokenIssued join_voice(const ChannelId& channel,
                                                         const UserId& user);

    /// Called when the last participant leaves a channel.
    /// Cleans up the E2EE key and releases the node binding.
    void on_channel_empty(const ChannelId& channel);

    /// Admin action: eject a participant from the LiveKit room.
    bool kick_user(const ChannelId& channel, const UserId& target);

    /// Called for each incoming LiveKit webhook event.
    void on_livekit_event(const livekit::webhook::LiveKitEvent& event);

    /// Called once during bootstrap to recover voice state after a server restart.
    /// Loads mute/deafen preferences from DB, clears DB, then kicks all LiveKit
    /// participants so they reconnect with fresh tokens and E2EE keys.
    void recover_from_restart();

    /// Mark that the next participant_left webhook for this user likely belongs
    /// to the replaced owner session during takeover.
    void mark_takeover(const UserId& user);

    /// Mark that a ROOM_FINISHED webhook for this channel should be ignored
    /// because a takeover is in progress and the new session hasn't connected yet.
    void mark_channel_takeover(const ChannelId& channel);

    /// Persist voice join: applies any pending recovery preferences, then upserts to DB.
    void persist_voice_join(const UserId& user, const ChannelId& channel);

    /// Persist voice leave: removes the user's row from DB.
    void persist_voice_leave(const UserId& user);

    /// Persist mute/deafen state change to DB.
    void persist_mute_state(const UserId& user, bool muted, bool deafened);

    /// Access the voice session state.
    VoiceSessionManager& sessions() { return sessions_; }
    const VoiceSessionManager& sessions() const { return sessions_; }

   private:
    bool consume_takeover_left_guard(const UserId& user);
    bool consume_channel_takeover_guard(const ChannelId& channel);
    ParticipantState consume_pending_preferences(const UserId& user);

    VoiceStateRepository& voice_state_repo_;
    livekit::LiveKitTokenService token_service_;
    livekit::LivekitNodeRegistry nodes_;
    livekit::E2EEKeyManager e2ee_keys_;
    VoiceSessionManager sessions_;
    std::mutex takeover_guard_mutex_;
    std::unordered_map<UserId, std::chrono::steady_clock::time_point> takeover_left_guard_;
    std::unordered_map<ChannelId, std::chrono::steady_clock::time_point> channel_takeover_guard_;
    std::unordered_map<UserId, ParticipantState> pending_preferences_;

    static constexpr std::chrono::seconds kTakeoverGuardTtl{45};
};

}  // namespace app::services::voice

#endif  // APP_VOICE_VOICESERVICE_H_
