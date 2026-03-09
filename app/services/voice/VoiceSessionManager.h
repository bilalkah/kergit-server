#ifndef APP_SERVICES_VOICE_VOICESESSIONMANAGER_H_
#define APP_SERVICES_VOICE_VOICESESSIONMANAGER_H_

#include "app/managers/session/SessionId.h"
#include "domains/ids/Ids.h"

#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace app::services::voice {

struct ParticipantState {
    bool muted = false;
    bool deafened = false;
};

struct UserVoiceState {
    ChannelId channel;
    SessionId owner_session;
};

/**
 * VoiceSessionManager
 *
 * Single source of truth for voice channel participation.
 * Updated by webhook events (join/leave) and client commands (mute/deafen).
 *
 * Tracks:
 *  - Which users are in which voice channels
 *  - Which session owns each user's voice connection
 *  - Mute/deafen states per user
 */
class VoiceSessionManager {
public:

    /// User joins a voice channel.
    /// Returns true if this is the first participant in the channel.
    bool join(const ChannelId& channel, const UserId& user, const SessionId& session);

    /// User leaves a voice channel.
    /// Returns true if the channel becomes empty.
    bool leave(const ChannelId& channel, const UserId& user);

    /// Remove a user from whatever channel they're in.
    /// Returns the channel they were in and whether it became empty.
    struct LeaveResult {
        std::optional<ChannelId> channel;
        bool became_empty = false;
    };
    LeaveResult leave_user(const UserId& user);

    /// Remove a user only if the provided session currently owns voice.
    /// Returns whether removal happened, plus channel/empty details.
    struct LeaveIfOwnerResult {
        bool removed = false;
        std::optional<ChannelId> channel;
        bool became_empty = false;
    };
    LeaveIfOwnerResult leave_if_owner(const UserId& user, const SessionId& expected_owner_session);

    /// Update mute state. Returns true if changed.
    bool set_muted(const UserId& user, bool muted);

    /// Update deafened state (also sets muted when deafening). Returns true if changed.
    bool set_deafened(const UserId& user, bool deafened);

    /// Get the channel a user is currently in.
    std::optional<ChannelId> user_channel(const UserId& user) const;

    /// Get the owning session for a user's voice connection.
    std::optional<SessionId> user_session(const UserId& user) const;

    /// Returns participants in a channel with their states.
    struct ParticipantInfo {
        UserId user_id;
        bool muted = false;
        bool deafened = false;
    };
    std::vector<ParticipantInfo> participants_in_channel(const ChannelId& channel) const;

    /// Returns just user IDs in a channel.
    std::vector<UserId> users_in_channel(const ChannelId& channel) const;

    /// Clear all participants in a channel and remove their user->channel mappings.
    /// Returns removed user ids.
    std::vector<UserId> clear_channel(const ChannelId& channel);

    /// Returns true if channel has no participants.
    bool is_empty(const ChannelId& channel) const;

private:
    mutable std::mutex mutex_;

    // channel -> { user -> state }
    std::unordered_map<ChannelId, std::unordered_map<UserId, ParticipantState>> channels_;

    // user -> { channel, session }
    std::unordered_map<UserId, UserVoiceState> user_to_channel_;
};

} // namespace app::services::voice

#endif // APP_SERVICES_VOICE_VOICESESSIONMANAGER_H_
