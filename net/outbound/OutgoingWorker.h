#ifndef NET_OUTBOUND_OUTGOINGWORKER_H
#define NET_OUTBOUND_OUTGOINGWORKER_H

#include "net/connection/ConnectionRegistery.h"
#include "net/outbound/OutgoingQueue.h"
#include "net/transport/ILoop.h"
#include "net/transport/IOutboundTransport.h"
#include "utils/Loggable.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <unordered_set>

struct us_timer_t;

namespace net::outbound {

struct OutgoingWorkerConfig {
    std::chrono::milliseconds interval{5};
    std::chrono::microseconds time_budget{1000};
    std::size_t max_per_tick{256};

    // Reliable-delivery retransmit policy. RTO is intentionally well under the 5s
    // heartbeat: clients ack immediately on receive, so a frame only sits unacked for
    // ~1 RTT in the happy path and we resend quickly when an ack is genuinely lost.
    std::chrono::milliseconds retransmit_timeout{500};
    uint16_t max_retransmits{8};  // after this many tries, drop -> reconnect -> resync
};

class OutgoingWorker : public utils::Loggable {
   public:
    OutgoingWorker(transport::ILoop& loop, connection::ConnectionRegistery& conns,
                   transport::IOutboundTransport& transport, OutgoingQueue& out_q,
                   OutgoingWorkerConfig cfg = {});
    ~OutgoingWorker();

    void start();
    void stop();

    // Cumulative ack from a client: drop every buffered reliable frame with
    // seq <= ack_seq for this connection. Called on the event-loop thread from the
    // transport ACK fast-path (and heartbeat backstop), so it shares the loop thread
    // with tick()/retransmit and needs no extra synchronization for the unacked index.
    void on_ack(const ConnId& conn_id, uint64_t ack_seq);

   private:
    static void on_timer(us_timer_t* timer);

    // tick() orchestrates one pass; the steps below are its pieces:
    void tick();
    // Drain the shared queue (bounded by max_per_tick / time budget), fanning each message into the
    // target connections' outboxes. Returns how many queue messages were processed.
    std::size_t drain_queue(std::unordered_set<ConnId>& touched);
    // Fan one message out to all of its target connections.
    void distribute(const OutgoingMessage& msg, std::unordered_set<ConnId>& touched);
    // Append one message to a single connection's bounded outbox (with overflow/drop policy).
    void enqueue_to_outbox(const GlobalConnId& global_id, const OutgoingMessage& msg,
                           std::unordered_set<ConnId>& touched);
    // Flush one connection's queued outbox to the socket, deferring any DropConnection until after
    // the registry lock is released (handle.end() re-enters the registry via the close callback).
    void flush_connection_outbox(const ConnId& conn_id);
    // Send a single SendPayload action (reliable seq-stamp+buffer, or plain send). Returns whether
    // the action was consumed; false means "backpressured / send raced — retry next tick".
    bool send_payload(connection::ConnectionContext& ctx, const ConnId& conn_id,
                      const SendPayload& action);
    // Resend reliable frames whose ack is overdue; drop connections that exhaust retries.
    void retransmit_sweep();

    /**
     * References
     */
    transport::ILoop& loop_;
    connection::ConnectionRegistery& conns_;
    transport::IOutboundTransport& transport_;
    OutgoingQueue& out_q_;

    /**
     * Configuration
     */
    OutgoingWorkerConfig cfg_;

    /**
     * Runtime state
     */
    std::atomic<bool> running_{false};
    ::us_timer_t* timer_{nullptr};
    bool tick_deadline_enabled_{false};
    std::chrono::steady_clock::time_point tick_deadline_{};

    // Connections that currently hold unacked reliable frames. Maintained purely on the
    // loop thread (insert on reliable send, erase when the buffer drains via ack/drop) so
    // the retransmit sweep only visits connections that actually need it.
    std::unordered_set<ConnId> pending_unacked_conns_{};
};

};  // namespace net::outbound

#endif  // NET_OUTBOUND__OUTGOINGWORKER_H
