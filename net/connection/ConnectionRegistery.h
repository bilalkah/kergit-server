#ifndef NET_CONNECTION_CONNECTIONREGISTERY_H
#define NET_CONNECTION_CONNECTIONREGISTERY_H

#include "domains/ids/Ids.h"
#include "net/Types.h"
#include "net/transport/Handle.h"

#include <deque>
#include <expected>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace net::connection {

/**
 * State used for heartbeat management.
 */
struct HeartbeatState {
    bool alive{false};
    std::chrono::system_clock::time_point connected_at{std::chrono::system_clock::now()};
    std::chrono::system_clock::time_point last_ping_at{std::chrono::system_clock::now()};
    std::chrono::system_clock::time_point last_pong_at{std::chrono::system_clock::now()};
    std::chrono::milliseconds rtt_ms{0};
};

/**
 * State associated with authentication.
 */
struct AuthState {
    bool is_authenticated{false};
    std::chrono::system_clock::time_point expires_at{};
};

/**
 * Context associated with a connection.
 */
struct ConnectionContext {
    ConnectionContext() = default;
    explicit ConnectionContext(const ConnId conn, const transport::WsHandle h,
                               const TransportKind k)
        : conn_id(conn), handle(h), kind(k) {}

    // Connection identifier
    ConnId conn_id{""};

    // Underlying transport handle
    transport::WsHandle handle{};

    // Transport type
    TransportKind kind{TransportKind::TextWebSocket};

    // Heartbeat state
    HeartbeatState heartbeat{};

    // Authentication state
    AuthState auth{};

    // deque for backpressured outgoing messages
    std::deque<std::pair<std::string, uWS::OpCode>> pending;
};

/**
 * Error type for connection operations.
 */
struct ConnectionError {
    ConnectionError() = default;
    explicit ConnectionError(std::string msg) : message(std::move(msg)) {}
    std::string message{""};
};

/**
 * Result type for connection operations.
 */
using ConnectionResult = std::expected<ConnectionContext, ConnectionError>;

/**
 * Registry managing active connections.
 */
class ConnectionRegistery {
   public:
    /**
     * Attach/Detach a new connection to the registry.
     */
    void attach(const ConnId& conn_id, ConnectionContext context = {});
    void detach(const ConnId& conn_id);

    /**
     * Get read-only connection context(s) by connection ID(s).
     */
    ConnectionResult get(const ConnId& conn_id) const;
    std::vector<ConnectionResult> get(const std::vector<GlobalConnId>& global_ids) const;
    std::vector<ConnectionResult> get() const;

    /**
     * Mutate in-place (no copies)
     */
    std::expected<void, ConnectionError> mutate(
        const ConnId& conn_id, const std::function<void(ConnectionContext&)>& mutator);

    /**
     * Get the number of active connections.
     * @return size_t Number of active connections.
     */
    size_t size() const;

   private:
    /**
     * Mutex to protect access to the connection map.
     * Readers-writer lock could be used here for better read performance if needed.
     */
    mutable std::shared_mutex mutex_;

    /**
     * Map from connection ID to connection context (which includes the UwsSocket pointer).
     */
    std::unordered_map<ConnId, ConnectionContext> connections_;
};

}  // namespace net::connection

#endif  // NET_CONNECTION_CONNECTIONREGISTERY_H
