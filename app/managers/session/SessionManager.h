#ifndef APP_MANAGERS_SESSION_SESSIONMANAGER_H
#define APP_MANAGERS_SESSION_SESSIONMANAGER_H

#include "app/managers/session/SessionId.h"
#include "app/managers/session/SessionInfo.h"
#include "proto/event/error.pb.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace app {

using SessionError = std::string;

struct VoiceParticipantState {
    UserId user_id;
    bool muted = false;
    bool deafened = false;
};

/**
 * SessionManager owns ACTIVE logical session state.
 *
 * Invariants:
 *  - Sessions are created ONLY after auth
 *  - GlobalConnId is transport-level only
 *  - SessionId is internal logical app-session identity (server-only)
 *  - Voice ownership is user-owned and SessionId-scoped
 */
class SessionManager final {
   public:
    explicit SessionManager(std::size_t max_sessions_per_user = 0);

    // ---- lifecycle ----

    // Called on auth success. Always server-mints the logical session id.
    std::expected<SessionId, sercom::protocol::event::CommandErrorCode> attachConnection(
        const GlobalConnId& conn, const UserId& user);

    // Called on DisconnectionEvent
    void removeConnection(const GlobalConnId& conn);

    // ---- active operations ----
    void joinTextChannel(const UserId& session, const HubId& hub);
    void leaveTextChannel(const UserId& session);
    void stageVoiceJoin(const UserId& session, const SessionId& owner_session_id,
                        const ChannelId& channel);
    bool commitStagedVoiceJoin(const UserId& session, const SessionId& owner_session_id,
                               const ChannelId& channel);
    bool clearStagedVoiceJoinIfOwnedBy(const UserId& session, const SessionId& owner_session_id);
    void joinVoiceChannel(const UserId& session, const SessionId& owner_session_id,
                          const ChannelId& channel);
    void leaveVoiceChannel(const UserId& session);
    bool leaveVoiceChannelIfOwnedBy(const UserId& session, const SessionId& owner_session_id);
    bool setVoiceMuted(const UserId& session, bool muted);
    bool setVoiceDeafened(const UserId& session, bool deafened);
    std::vector<UserId> voiceParticipantsInChannel(const ChannelId& channel) const;
    std::vector<VoiceParticipantState> voiceParticipantStatesInChannel(
        const ChannelId& channel) const;

    // ---- queries ----
    // Returns true if user has at least one active transport connection.
    bool hasSession(const UserId& user) const;
    std::expected<UserId, SessionError> sessionOfConnection(const GlobalConnId& conn) const;
    std::expected<SessionId, SessionError> sessionIdOfConnection(const GlobalConnId& conn) const;
    const std::expected<SessionInfo, SessionError> getSession(const UserId& session) const;
    const std::expected<SessionId, SessionError> getVoiceOwnerSessionId(
        const UserId& session) const;
    bool isVoiceOwnedBySession(const UserId& session, const SessionId& session_id) const;

    // All active transport connections for the user.
    std::vector<GlobalConnId> getSessionConnections(const UserId& session) const;
    // Active transport connections attached to this internal session id.
    std::vector<GlobalConnId> getSessionIdConnections(const SessionId& session_id) const;
    // Active logical session ids for this user.
    std::vector<SessionId> getUserSessionIds(const UserId& user) const;
    bool sessionIdHasConnections(const SessionId& session_id) const;

    std::vector<UserId> activeUsers() const;

   private:
    SessionId mintSessionIdLocked();
    bool userHasActiveSessionsLocked(const UserId& user) const;
    std::vector<GlobalConnId> collectUserConnectionsLocked(const UserId& user) const;

    // Single lock protecting session maps; do not hold across external calls.
    mutable std::shared_mutex mutex_;
    const std::size_t max_sessions_per_user_;
    std::atomic<uint64_t> next_session_id_{0};

    // Primary storage (user-owned state)
    std::unordered_map<UserId, SessionInfo> sessions_;

    // Transport -> user
    std::unordered_map<GlobalConnId, UserId> conn_to_session_;

    // Transport -> internal session id
    std::unordered_map<GlobalConnId, SessionId> conn_to_session_id_;

    // Internal session id -> user owner
    std::unordered_map<SessionId, UserId> session_id_to_user_;

    // Internal session id -> active transport connections
    std::unordered_map<SessionId, std::unordered_set<GlobalConnId>> session_id_to_conns_;

    // User -> internal session ids
    std::unordered_map<UserId, std::unordered_set<SessionId>> user_to_session_ids_;
};

}  // namespace app

#endif  // APP_MANAGERS_SESSION_SESSIONMANAGER_H
