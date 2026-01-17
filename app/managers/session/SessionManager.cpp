#include "app/managers/session/SessionManager.h"

namespace app {

void SessionManager::createSession(const GlobalConnId& main_conn, const UserId& user) {
    std::unique_lock lock(mutex_);
    UserId session = user;

    SessionInfo info;
    info.main_conn = main_conn;

    sessions_.emplace(session, std::move(info));
    conn_to_session_[main_conn] = session;
}

/**
 * Removes a connection from the session manager.
 * It will remove the entire session.
 */
void SessionManager::removeConnection(const GlobalConnId& conn) {
    std::unique_lock lock(mutex_);
    auto it = conn_to_session_.find(conn);
    if (it == conn_to_session_.end()) {
        return;  // connection not found
    }

    const UserId session = it->second;
    auto sit = sessions_.find(session);
    if (sit == sessions_.end()) {
        conn_to_session_.erase(it);
        return;  // session not found, clean up mapping
    }

    // Remove voice connection mapping if exists
    if (sit->second.voice_conn) {
        conn_to_session_.erase(*sit->second.voice_conn);
    }

    // Remove main connection mapping
    conn_to_session_.erase(it);

    // Remove session
    sessions_.erase(sit);
}

void SessionManager::joinTextChannel(const UserId& session, const HubId& hub,
                                     const ChannelId& channel) {
    std::unique_lock lock(mutex_);
    auto it = sessions_.find(session);
    if (it == sessions_.end()) return;

    it->second.current_hub = hub;
    it->second.current_text_channel = channel;
}

void SessionManager::leaveTextChannel(const UserId& session) {
    std::unique_lock lock(mutex_);
    auto it = sessions_.find(session);
    if (it == sessions_.end()) return;

    it->second.current_text_channel.reset();
    it->second.current_hub.reset();
}

void SessionManager::attachVoice(const UserId& session, const GlobalConnId& voice_conn,
                                 const ChannelId& channel) {
    std::unique_lock lock(mutex_);
    auto it = sessions_.find(session);
    if (it == sessions_.end()) return;

    it->second.voice_conn = voice_conn;
    it->second.current_voice_channel = channel;
    conn_to_session_[voice_conn] = session;
}

void SessionManager::detachVoice(const UserId& session) {
    std::unique_lock lock(mutex_);
    auto it = sessions_.find(session);
    if (it == sessions_.end()) return;

    if (it->second.voice_conn) {
        conn_to_session_.erase(*it->second.voice_conn);
        it->second.voice_conn.reset();
        it->second.current_voice_channel.reset();
    }
}

bool SessionManager::hasSession(const UserId& user) const {
    std::shared_lock lock(mutex_);
    UserId session = user;  // by design
    return sessions_.contains(session);
}

std::expected<UserId, SessionError> SessionManager::sessionOfConnection(
    const GlobalConnId& conn) const {
    std::shared_lock lock(mutex_);
    auto it = conn_to_session_.find(conn);
    if (it == conn_to_session_.end()) return std::unexpected<SessionError>("Connection not found");
    return it->second;
}

const std::expected<SessionInfo, SessionError> SessionManager::getSession(
    const UserId& session) const {
    std::shared_lock lock(mutex_);
    auto it = sessions_.find(session);
    if (it == sessions_.end()) return std::unexpected<SessionError>("Session not found");
    return it->second;
}

const std::expected<GlobalConnId, SessionError> SessionManager::getMainConnection(
    const UserId& session) const {
    auto it = sessions_.find(session);
    if (it == sessions_.end()) return std::unexpected<SessionError>("Session not found");
    if (!it->second.main_conn)
        return std::unexpected<SessionError>("Main connection not found for session");
    return *(it->second.main_conn);
}

std::vector<UserId> SessionManager::activeUsers() const {
    std::shared_lock lock(mutex_);
    std::vector<UserId> users;
    users.reserve(sessions_.size());
    for (const auto& [UserId, info] : sessions_) {
        users.push_back(UserId);
    }
    return users;
}

}  // namespace app
