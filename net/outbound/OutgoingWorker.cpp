#include "net/outbound/OutgoingWorker.h"

#include "utils/Metrics.h"

#include <algorithm>
#include <cassert>
#include <deque>
#include <unordered_set>

namespace net::outbound {
// OutgoingWorker is the single owner of per-connection outbox execution.
// All outbound actions (SendPayload, UpdateAuthState, DropConnection)
// are executed here on the event loop thread.
OutgoingWorker::OutgoingWorker(transport::ILoop& loop, connection::ConnectionRegistery& conns,
                               transport::IOutboundTransport& transport, OutgoingQueue& out_q,
                               OutgoingWorkerConfig cfg)
    : loop_(loop), conns_(conns), transport_(transport), out_q_(out_q), cfg_(std::move(cfg)) {}

OutgoingWorker::~OutgoingWorker() { stop(); }

void OutgoingWorker::start() {
    if (running_.exchange(true)) return;
    auto* loop = loop_.getUwsLoop();
    if (!loop) {
        running_.store(false);
        return;
    }
    timer_ = us_create_timer(reinterpret_cast<us_loop_t*>(loop), 0, sizeof(OutgoingWorker*));
    if (!timer_) {
        running_.store(false);
        return;
    }
    auto** slot = reinterpret_cast<OutgoingWorker**>(us_timer_ext(timer_));
    if (slot) *slot = this;
    us_timer_set(timer_, &OutgoingWorker::on_timer, static_cast<int>(cfg_.interval.count()),
                 static_cast<int>(cfg_.interval.count()));
}

void OutgoingWorker::stop() {
    auto* timer = timer_;
    if (!timer) {
        running_.store(false);
        return;
    }
    timer_ = nullptr;
    running_.store(false);

    if (auto* loop = loop_.getUwsLoop()) {
        loop->defer([timer]() {
            if (!timer) return;
            auto** slot = reinterpret_cast<OutgoingWorker**>(us_timer_ext(timer));
            if (slot) *slot = nullptr;
            us_timer_set(timer, nullptr, 0, 0);
            us_timer_close(timer);
        });
    } else {
        auto** slot = reinterpret_cast<OutgoingWorker**>(us_timer_ext(timer));
        if (slot) *slot = nullptr;
        us_timer_set(timer, nullptr, 0, 0);
        us_timer_close(timer);
    }
}

void OutgoingWorker::on_timer(us_timer_t* timer) {
    auto** slot = reinterpret_cast<OutgoingWorker**>(us_timer_ext(timer));
    OutgoingWorker* self = slot ? *slot : nullptr;
    if (self) {
        self->tick();
    }
}

void OutgoingWorker::flush_connection_outbox(connection::ConnectionContext& ctx) {
    while (!ctx.outbox.q.empty()) {
        if (tick_deadline_enabled_ && std::chrono::steady_clock::now() >= tick_deadline_) {
            return;
        }

        const std::size_t outbox_size_before = ctx.outbox.q.size();
        auto& msg = ctx.outbox.q.front();
        bool should_continue = false;
        bool popped_one = false;
        std::visit(
            [&](auto& action) {
                using T = std::decay_t<decltype(action)>;
                if constexpr (std::is_same_v<T, SendPayload>) {
                    if (!action.payload.data) {
                        return;
                    }
                    if (transport_.is_backpressured(ctx.handle)) {
                        utils::metrics::counters().outbound_backpressured_total.fetch_add(
                            1, std::memory_order_relaxed);
                        return;
                    }
                    assert(ctx.handle.valid() &&
                           "OutgoingWorker invariant: handle must be valid before send");
                    if (!ctx.handle.valid()) {
                        utils::metrics::counters().outbound_flush_send_fail_total.fetch_add(
                            1, std::memory_order_relaxed);
                        return;
                    }
                    const auto& bytes = *action.payload.data;
                    const bool ok = transport_.send(ctx.handle, bytes, action.payload.is_binary);
                    if (ok) {
                        ctx.outbox.slow_hits = 0;
                        ctx.outbox.q.pop_front();
                        popped_one = true;
                        utils::metrics::counters().outbound_flush_total.fetch_add(
                            1, std::memory_order_relaxed);
                        should_continue = true;
                        return;
                    }
                    utils::metrics::counters().outbound_flush_send_fail_total.fetch_add(
                        1, std::memory_order_relaxed);
                    return;
                } else if constexpr (std::is_same_v<T, UpdateAuthState>) {
                    ctx.auth.status = action.status;
                    ctx.auth.expires_at = action.expires_at;
                    ctx.outbox.slow_hits = 0;
                    ctx.outbox.q.pop_front();
                    popped_one = true;
                    utils::metrics::counters().outbound_update_auth_state_total.fetch_add(
                        1, std::memory_order_relaxed);
                    should_continue = true;
                    return;
                } else if constexpr (std::is_same_v<T, DropConnection>) {
                    ctx.handle.end(action.code, action.reason);
                    ctx.outbox.q.clear();
                    ctx.outbox.slow_hits = 0;
                    assert(ctx.outbox.q.empty() &&
                           "OutgoingWorker invariant: DropConnection must clear outbox");
                    utils::metrics::counters().outbound_drop_connection_total.fetch_add(
                        1, std::memory_order_relaxed);
                    return;
                }
            },
            msg.action);

        if (!should_continue) {
            return;
        }

        assert(popped_one &&
               "OutgoingWorker invariant: continued flush iteration must pop exactly one item");
        assert(ctx.outbox.q.size() + 1 == outbox_size_before &&
               "OutgoingWorker invariant: continued flush iteration must pop exactly one item");
        if (!popped_one || ctx.outbox.q.size() + 1 != outbox_size_before) {
            // Runtime guard for release builds to prevent accidental spin on invariant breaks.
            return;
        }
    }
}

/**
 * Outgoing worker tick: process outgoing message queue
 * - send messages to connections as per the message type
 * - handle direct messages and publish messages
 */
void OutgoingWorker::tick() {
    const auto start = std::chrono::steady_clock::now();
    tick_deadline_enabled_ = cfg_.time_budget.count() > 0;
    tick_deadline_ = start + cfg_.time_budget;
    std::size_t processed = 0;
    std::unordered_set<ConnId> touched_connections;
    touched_connections.reserve(cfg_.max_per_tick);

    auto enqueue_to_connection = [&](const GlobalConnId& global_id, const OutgoingMessage& msg) {
        OutgoingMessage per_conn;
        per_conn.priority = msg.priority;
        per_conn.target = Target::one(global_id);
        per_conn.action = msg.action;

        auto result = conns_.mutate(global_id.conn_id, [&](auto& ctx) {
            auto& ob = ctx.outbox;
            if (ob.q.size() < ob.capacity) {
                ob.q.push_back(std::move(per_conn));
                utils::metrics::counters().per_conn_queue_enqueued_total.fetch_add(
                    1, std::memory_order_relaxed);
                return;
            }

            if (per_conn.priority == OutboundPriority::Low) {
                utils::metrics::counters().per_conn_queue_dropped_low_total.fetch_add(
                    1, std::memory_order_relaxed);
                return;
            }

            auto rit = std::find_if(ob.q.rbegin(), ob.q.rend(), [](const OutgoingMessage& m) {
                return m.priority == OutboundPriority::Low;
            });
            if (rit != ob.q.rend()) {
                ob.q.erase(std::next(rit).base());
                utils::metrics::counters().per_conn_queue_dropped_low_total.fetch_add(
                    1, std::memory_order_relaxed);
                ob.q.push_back(std::move(per_conn));
                utils::metrics::counters().per_conn_queue_enqueued_total.fetch_add(
                    1, std::memory_order_relaxed);
                return;
            }

            utils::metrics::counters().per_conn_queue_overflow_total.fetch_add(
                1, std::memory_order_relaxed);
            ob.slow_hits += 1;
            if (ob.slow_hits >= 4) {
                ob.drop_pending = true;
                ob.q.clear();
            }
        });

        if (!result.has_value()) {
            utils::metrics::counters().registry_miss_total.fetch_add(1, std::memory_order_relaxed);
            utils::metrics::counters().dropped_outbound_total.fetch_add(1,
                                                                        std::memory_order_relaxed);
            return;
        }
        touched_connections.insert(global_id.conn_id);
    };

    auto distribute_message = [&](const OutgoingMessage& msg) {
        utils::metrics::counters().registry_copy_eliminated_total.fetch_add(
            1, std::memory_order_relaxed);
        if (msg.target.conns.size() > 1 && std::holds_alternative<SendPayload>(msg.action)) {
            utils::metrics::counters().fanout_payload_shared_total.fetch_add(
                1, std::memory_order_relaxed);
        }
        for (const auto& global_id : msg.target.conns) {
            enqueue_to_connection(global_id, msg);
        }
    };

    while (processed < cfg_.max_per_tick) {
        if (tick_deadline_enabled_ && std::chrono::steady_clock::now() >= tick_deadline_) {
            break;
        }

        OutgoingMessage msg;
        if (!out_q_.try_pop(msg)) {
            break;
        }

        distribute_message(msg);
        ++processed;
    }

    for (const auto& conn_id : touched_connections) {
        if (tick_deadline_enabled_ && std::chrono::steady_clock::now() >= tick_deadline_) {
            break;
        }
        // Check if there's a DropConnection action - must handle it outside the lock
        // because handle.end() triggers .close callback which calls conns_.detach()
        std::optional<DropConnection> pending_drop;
        transport::WsHandle drop_handle;

        conns_.mutate(conn_id, [&](connection::ConnectionContext& ctx) {
            // Process all non-DropConnection actions inside the lock
            while (!ctx.outbox.q.empty()) {
                auto& msg = ctx.outbox.q.front();
                bool processed = false;
                bool found_drop = false;

                std::visit(
                    [&](auto& action) {
                        using T = std::decay_t<decltype(action)>;
                        if constexpr (std::is_same_v<T, SendPayload>) {
                            if (!action.payload.data) {
                                processed = true;
                                return;
                            }
                            if (transport_.is_backpressured(ctx.handle)) {
                                return;
                            }
                            if (!ctx.handle.valid()) {
                                processed = true;
                                return;
                            }
                            if (transport_.send(ctx.handle, *action.payload.data, action.payload.is_binary)) {
                                utils::metrics::counters().outbound_flush_total.fetch_add(1, std::memory_order_relaxed);
                                processed = true;
                            }
                        } else if constexpr (std::is_same_v<T, UpdateAuthState>) {
                            ctx.auth.status = action.status;
                            ctx.auth.expires_at = action.expires_at;
                            utils::metrics::counters().outbound_update_auth_state_total.fetch_add(1, std::memory_order_relaxed);
                            processed = true;
                        } else if constexpr (std::is_same_v<T, DropConnection>) {
                            // Don't call handle.end() here - defer it outside the lock
                            pending_drop = action;
                            drop_handle = ctx.handle;
                            found_drop = true;
                            processed = true;
                        }
                    },
                    msg.action);

                if (found_drop) {
                    ctx.outbox.q.clear();
                    break;
                }
                if (processed) {
                    ctx.outbox.q.pop_front();
                } else {
                    break;  // Backpressured
                }
            }
        });

        // Handle DropConnection outside the lock to avoid deadlock
        if (pending_drop.has_value() && drop_handle.valid()) {
            drop_handle.end(pending_drop->code, pending_drop->reason);
            utils::metrics::counters().outbound_drop_connection_total.fetch_add(1, std::memory_order_relaxed);
        }
    }

    utils::metrics::observe_outbound_msgs_per_tick(processed);
    utils::metrics::maybe_log();
}

}  // namespace net::outbound
