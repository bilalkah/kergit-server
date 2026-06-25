#ifndef NET_CONNECTION_CONNECTIONREGISTERY_H
#define NET_CONNECTION_CONNECTIONREGISTERY_H

#include "domains/ids/Ids.h"
#include "net/Types.h"
#include "net/outbound/Msg.h"
#include "net/transport/Handle.h"

#include <chrono>
#include <cstdint>
#include <deque>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace net::connection {

/**
 * State used for heartbeat / liveness management.
 *
 * Liveness is driven by the client's application-level PING (an Envelope, i.e. a
 * WebSocket DATA frame), NOT by WebSocket protocol ping/pong control frames. Control
 * frames can be answered by an intermediary proxy (Caddy/Cloudflare), so a pong does
 * not prove the real client is alive — only an end-to-end app PING does. `last_seen_at`
 * is therefore updated whenever an app PING arrives from the client; the heartbeat
 * sweep closes connections that go silent for longer than the configured timeout.
 */
struct HeartbeatState {
    std::chrono::system_clock::time_point connected_at{std::chrono::system_clock::now()};
    std::chrono::system_clock::time_point last_seen_at{std::chrono::system_clock::now()};
};

/**
 * State associated with authentication.
 */
enum class AuthState : uint8_t {
    AUTH_PENDING = 0,
    AUTHENTICATED = 1,
    AUTH_FAILED = 2,
};

/**
 * Context associated with a connection.
 */
struct ConnectionContext {
    ConnectionContext() = default;
    explicit ConnectionContext(const ConnId conn, const transport::WsHandle h,
                               const TransportKind k, std::size_t port_idx = 0)
        : conn_id(conn), handle(h), kind(k), port_index(port_idx) {}

    // Connection identifier
    ConnId conn_id{""};

    // Underlying transport handle
    transport::WsHandle handle{};

    // Transport type
    TransportKind kind{TransportKind::TextWebSocket};

    // Port index for metrics tracking
    std::size_t port_index{0};

    // Heartbeat state
    HeartbeatState heartbeat{};

    // Authentication state
    AuthState auth_state{AuthState::AUTH_PENDING};
    std::chrono::system_clock::time_point auth_expires_at{};
    std::optional<UserId> user_id{};

    // deque for backpressured outgoing messages
    std::deque<std::pair<std::string, uWS::OpCode>> pending;

    struct PerConnectionOutbox {
        std::deque<net::outbound::OutgoingMessage> q;
        std::size_t capacity{128};
        uint32_t slow_hits{0};
        bool drop_pending{false};
    };

    PerConnectionOutbox outbox{};

    // Reliable (at-least-once) outbound delivery state.
    //
    // Sequenced frames are stamped with a per-connection monotonic `seq`, sent, and
    // retained here until the client acknowledges them (cumulative ack). The server
    // retransmits unacked frames on timeout and drops the connection if a frame is
    // never acked. All fields are mutated only on the event-loop thread (OutgoingWorker
    // flush/retransmit and the transport ACK fast-path), so they need no extra locking
    // beyond the registry mutate() that guards the surrounding ConnectionContext.
    struct ReliableOutbound {
        struct Pending {
            uint64_t seq{0};
            std::shared_ptr<const std::string> bytes;  // exact wire bytes (seq included)
            std::chrono::steady_clock::time_point last_sent_at{};
            uint16_t attempts{0};
        };

        uint64_t next_seq{0};        // highest seq assigned so far (++ before use)
        uint64_t acked_seq{0};       // highest cumulative ack received from the client
        std::deque<Pending> buffer;  // unacked frames, ascending by seq
        std::size_t capacity{256};   // hard cap; overflow => drop+reconnect+resync
    };

    ReliableOutbound reliable{};
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

struct ConnectionView {
    ConnId conn_id{""};
    transport::WsHandle handle{};
    TransportKind kind{TransportKind::TextWebSocket};
    AuthState auth_state{AuthState::AUTH_PENDING};
    std::chrono::system_clock::time_point auth_expires_at{};
    std::optional<UserId> user_id{};
};

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
    // Hot-path accessor.
    // Returns a lightweight snapshot of connection state without copying
    // any containers. No lock is held beyond this call.
    std::optional<ConnectionView> get_view(const ConnId& conn_id) const;
    std::vector<ConnId> get_ids() const;

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
