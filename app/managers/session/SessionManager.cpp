#include "app/managers/session/SessionManager.h"

#include "utils/EventLogger.h"
#include "utils/Metrics.h"

#include <string>
#include <string_view>

namespace app {
namespace {

std::string format_connection_id(const GlobalConnId& conn) {
    return conn.netstack_id.value + ":" + conn.conn_id.value;
}

std::string format_session_details(const GlobalConnId& conn, SessionId session_id) {
    return "conn_id=" + format_connection_id(conn) + " session_id=" + std::to_string(session_id);
}

void log_session_event(const UserId& user, std::string_view event, const std::string& details) {
    utils::EventLogger::instance().log(utils::EventCategory::SESSION, user.value, event, 0,
                                       details);
}

}  // namespace

SessionManager::SessionManager(std::size_t max_sessions_per_user)
    : max_sessions_per_user_(max_sessions_per_user) {}

SessionId SessionManager::mintSessionIdLocked() { return ++next_session_id_; }

bool SessionManager::userHasActiveSessionsLocked(const UserId& user) const {
    auto it = user_to_session_ids_.find(user);
    return it != user_to_session_ids_.end() && !it->second.empty();
}

std::vector<GlobalConnId> SessionManager::collectUserConnectionsLocked(const UserId& user) const {
    std::vector<GlobalConnId> conns;
    auto sessions_it = user_to_session_ids_.find(user);
    if (sessions_it == user_to_session_ids_.end()) return conns;

    for (const auto& session_id : sessions_it->second) {
        auto conn_it = session_id_to_conns_.find(session_id);
        if (conn_it == session_id_to_conns_.end()) continue;
        conns.insert(conns.end(), conn_it->second.begin(), conn_it->second.end());
    }
    return conns;
}

std::expected<SessionId, sercom::protocol::event::CommandErrorCode>
SessionManager::attachConnection(const GlobalConnId& conn, const UserId& user) {
    std::unique_lock lock(mutex_);

    if (auto existing = conn_to_session_id_.find(conn); existing != conn_to_session_id_.end()) {
        return existing->second;
    }

    auto user_sessions_it = user_to_session_ids_.find(user);
    if (max_sessions_per_user_ > 0 && user_sessions_it != user_to_session_ids_.end() &&
        user_sessions_it->second.size() >= max_sessions_per_user_) {
        return std::unexpected(sercom::protocol::event::CommandErrorCode_SESSION_LIMIT_EXCEEDED);
    }

    const bool had_active_sessions = userHasActiveSessionsLocked(user);
    sessions_.try_emplace(user);

    if (!had_active_sessions) {
        utils::metrics::counters().active_users.fetch_add(1, std::memory_order_relaxed);
    }

    const SessionId session_id = mintSessionIdLocked();
    conn_to_session_[conn] = user;
    conn_to_session_id_[conn] = session_id;
    session_id_to_user_[session_id] = user;
    session_id_to_conns_[session_id].insert(conn);
    user_to_session_ids_[user].insert(session_id);

    log_session_event(user, "session_created", format_session_details(conn, session_id));
    return session_id;
}

void SessionManager::removeConnection(const GlobalConnId& conn) {
    std::unique_lock lock(mutex_);

    auto user_it = conn_to_session_.find(conn);
    if (user_it == conn_to_session_.end()) {
        conn_to_session_id_.erase(conn);
        return;
    }

    const UserId user = user_it->second;
    conn_to_session_.erase(user_it);

    auto session_id_it = conn_to_session_id_.find(conn);
    if (session_id_it == conn_to_session_id_.end()) return;
    const SessionId session_id = session_id_it->second;
    conn_to_session_id_.erase(session_id_it);

    auto session_conns_it = session_id_to_conns_.find(session_id);
    if (session_conns_it != session_id_to_conns_.end()) {
        session_conns_it->second.erase(conn);
        if (!session_conns_it->second.empty()) return;
        session_id_to_conns_.erase(session_conns_it);
    }

    session_id_to_user_.erase(session_id);
    auto user_sessions_it = user_to_session_ids_.find(user);
    if (user_sessions_it != user_to_session_ids_.end()) {
        user_sessions_it->second.erase(session_id);
        if (user_sessions_it->second.empty()) {
            user_to_session_ids_.erase(user_sessions_it);
        }
    }

    log_session_event(user, "session_destroyed", "session_id=" + std::to_string(session_id));

    if (!userHasActiveSessionsLocked(user)) {
        sessions_.erase(user);
        utils::metrics::counters().active_users.fetch_sub(1, std::memory_order_relaxed);
    }
}

void SessionManager::joinTextChannel(const UserId& session, const HubId& hub) {
    std::unique_lock lock(mutex_);
    auto it = sessions_.find(session);
    if (it == sessions_.end()) return;
    it->second.current_hub = hub;
}

void SessionManager::leaveTextChannel(const UserId& session) {
    std::unique_lock lock(mutex_);
    auto it = sessions_.find(session);
    if (it == sessions_.end()) return;
    it->second.current_hub.reset();
}

bool SessionManager::hasSession(const UserId& user) const {
    std::shared_lock lock(mutex_);
    return userHasActiveSessionsLocked(user);
}

std::expected<UserId, SessionError> SessionManager::sessionOfConnection(
    const GlobalConnId& conn) const {
    std::shared_lock lock(mutex_);
    auto it = conn_to_session_.find(conn);
    if (it == conn_to_session_.end()) return std::unexpected<SessionError>("Connection not found");
    return it->second;
}

std::expected<SessionId, SessionError> SessionManager::sessionIdOfConnection(
    const GlobalConnId& conn) const {
    std::shared_lock lock(mutex_);
    auto it = conn_to_session_id_.find(conn);
    if (it == conn_to_session_id_.end()) {
        return std::unexpected<SessionError>("Session id not found for connection");
    }
    return it->second;
}

const std::expected<SessionInfo, SessionError> SessionManager::getSession(
    const UserId& session) const {
    std::shared_lock lock(mutex_);
    auto it = sessions_.find(session);
    if (it == sessions_.end()) return std::unexpected<SessionError>("Session not found");
    return it->second;
}

std::vector<GlobalConnId> SessionManager::getSessionConnections(const UserId& session) const {
    std::shared_lock lock(mutex_);
    return collectUserConnectionsLocked(session);
}

std::vector<GlobalConnId> SessionManager::getSessionIdConnections(
    const SessionId& session_id) const {
    std::shared_lock lock(mutex_);
    auto it = session_id_to_conns_.find(session_id);
    if (it == session_id_to_conns_.end()) return {};
    return {it->second.begin(), it->second.end()};
}

std::vector<SessionId> SessionManager::getUserSessionIds(const UserId& user) const {
    std::shared_lock lock(mutex_);
    auto it = user_to_session_ids_.find(user);
    if (it == user_to_session_ids_.end()) return {};
    return {it->second.begin(), it->second.end()};
}

bool SessionManager::sessionIdHasConnections(const SessionId& session_id) const {
    std::shared_lock lock(mutex_);
    auto it = session_id_to_conns_.find(session_id);
    return it != session_id_to_conns_.end() && !it->second.empty();
}

std::vector<UserId> SessionManager::activeUsers() const {
    std::shared_lock lock(mutex_);
    std::vector<UserId> users;
    users.reserve(user_to_session_ids_.size());
    for (const auto& [user_id, session_ids] : user_to_session_ids_) {
        if (!session_ids.empty()) users.push_back(user_id);
    }
    return users;
}

}  // namespace app
