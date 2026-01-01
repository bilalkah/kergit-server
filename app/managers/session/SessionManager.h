#ifndef APP_MANAGERS_SESSION_SESSIONMANAGER_H
#define APP_MANAGERS_SESSION_SESSIONMANAGER_H

#include "app/managers/session/SessionInfo.h"

#include <expected>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>

namespace app {

using SessionError = std::string;
/**
 * SessionManager owns ACTIVE runtime session state.
 *
 * Invariants:
 *  - Sessions are created ONLY after auth
 *  - GlobalConnId is transport-level only
 *  - Voice connections are owned by a session
 *  - Offline users leave zero memory
 */
class SessionManager final {
   public:
    // ---- lifecycle ----

    // Called on auth success
    void createSession(const GlobalConnId& main_conn, const UserId& user);

    // Called on DisconnectionEvent
    bool removeConnection(const GlobalConnId& conn);

    // ---- active operations ----
    void joinTextChannel(const UserId& session, const HubId& hub, const ChannelId& channel);
    void leaveTextChannel(const UserId& session);
    void attachVoice(const UserId& session, const GlobalConnId& voice_conn,
                     const ChannelId& channel);
    void detachVoice(const UserId& session);

    // ---- queries ----
    bool hasSession(const UserId& user) const;
    std::expected<UserId, SessionError> sessionOfConnection(const GlobalConnId& conn) const;
    const std::expected<SessionInfo, SessionError> getSession(const UserId& session) const;
    const std::expected<GlobalConnId, SessionError> getMainConnection(const UserId& session) const;
    const std::unordered_map<UserId, SessionInfo>& allSessions() const;
    std::vector<UserId> activeUsers() const;

   private:
    mutable std::shared_mutex mutex_;
    // Primary storage
    std::unordered_map<UserId, SessionInfo> sessions_;

    // Transport → session
    std::unordered_map<GlobalConnId, UserId> conn_to_session_;
};

}  // namespace app

#endif  // APP_MANAGERS_SESSION_SESSIONMANAGER_H
