#include "app/managers/session/SessionManager.h"
#include "utils/Metrics.h"

namespace app {

void SessionManager::createSession(const GlobalConnId& main_conn, const UserId& user) {
    std::unique_lock lock(mutex_);
    UserId session = user;

    // Check if session already exists - if so, don't add the new connection mapping
    if (sessions_.contains(session)) {
        return;  // Session already exists, don't overwrite or add new connection
    }

    SessionInfo info;
    info.main_conn = main_conn;

    sessions_.emplace(session, std::move(info));
    conn_to_session_[main_conn] = session;
    utils::metrics::counters().active_users.fetch_add(1, std::memory_order_relaxed);
}

bool SessionManager::tryCreateSession(const GlobalConnId& main_conn, const UserId& user) {
    std::unique_lock lock(mutex_);
    UserId session = user;

    // Atomic check-and-create: return false if session already exists
    if (sessions_.contains(session)) {
        return false;  // Session already exists for this user
    }

    SessionInfo info;
    info.main_conn = main_conn;

    sessions_.emplace(session, std::move(info));
    conn_to_session_[main_conn] = session;
    utils::metrics::counters().active_users.fetch_add(1, std::memory_order_relaxed);
    return true;
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

    // Remove main connection mapping
    conn_to_session_.erase(it);

    // Remove session
    sessions_.erase(sit);
    utils::metrics::counters().active_users.fetch_sub(1, std::memory_order_relaxed);
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

void SessionManager::joinVoiceChannel(const UserId& session, const HubId& hub,
                                      const ChannelId& channel) {
    std::unique_lock lock(mutex_);
    auto it = sessions_.find(session);
    if (it == sessions_.end()) return;

    it->second.current_voice_hub = hub;
    it->second.current_voice_channel = channel;
    it->second.voice_muted = false;
    it->second.voice_deafened = false;
}

void SessionManager::leaveVoiceChannel(const UserId& session) {
    std::unique_lock lock(mutex_);
    auto it = sessions_.find(session);
    if (it == sessions_.end()) return;

    it->second.current_voice_channel.reset();
    it->second.current_voice_hub.reset();
    it->second.voice_muted = false;
    it->second.voice_deafened = false;
}

bool SessionManager::setVoiceMuted(const UserId& session, bool muted) {
    std::unique_lock lock(mutex_);
    auto it = sessions_.find(session);
    if (it == sessions_.end()) return false;
    if (it->second.voice_muted == muted) return false;
    it->second.voice_muted = muted;
    return true;
}

bool SessionManager::setVoiceDeafened(const UserId& session, bool deafened) {
    std::unique_lock lock(mutex_);
    auto it = sessions_.find(session);
    if (it == sessions_.end()) return false;
    bool changed = false;
    if (it->second.voice_deafened != deafened) {
        it->second.voice_deafened = deafened;
        changed = true;
    }
    const bool desiredMuted = deafened ? true : false;
    if (it->second.voice_muted != desiredMuted) {
        it->second.voice_muted = desiredMuted;
        changed = true;
    }
    return changed;
}

std::vector<UserId> SessionManager::voiceParticipantsInChannel(const HubId& hub,
                                                               const ChannelId& channel) const {
    std::shared_lock lock(mutex_);
    std::vector<UserId> users;
    for (const auto& [user_id, info] : sessions_) {
        if (!info.current_voice_hub || !info.current_voice_channel) continue;
        if (info.current_voice_hub.value() != hub) continue;
        if (info.current_voice_channel.value() != channel) continue;
        users.push_back(user_id);
    }
    return users;
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
    std::shared_lock lock(mutex_);
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
