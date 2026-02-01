#include "net/outbound/OutgoingWorker.h"
#include "utils/Metrics.h"

#include <algorithm>

namespace net::outbound {
OutgoingWorker::OutgoingWorker(transport::ILoop& loop, connection::ConnectionRegistery& conns,
                               OutgoingQueue& out_q, OutgoingWorkerConfig cfg)
    : loop_(loop), conns_(conns), out_q_(out_q), cfg_(std::move(cfg)) {}

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

/**
 * Outgoing worker tick: process outgoing message queue
 * - send messages to connections as per the message type
 * - handle direct messages and publish messages
 */
void OutgoingWorker::tick() {
    const auto start = std::chrono::steady_clock::now();
    std::size_t processed = 0;

    auto enqueue_to_connection = [&](const GlobalConnId& global_id,
                                     const OutgoingMessage& msg) {
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

            auto rit = std::find_if(ob.q.rbegin(), ob.q.rend(),
                                    [](const OutgoingMessage& m) {
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
                if (ctx.handle.valid()) {
                    ctx.handle.end(1013, "slow consumer");
                }
                utils::metrics::counters().slow_connection_dropped_total.fetch_add(
                    1, std::memory_order_relaxed);
                ob.q.clear();
            }
        });

        if (!result.has_value()) {
            utils::metrics::counters().registry_miss_total.fetch_add(
                1, std::memory_order_relaxed);
            utils::metrics::counters().dropped_outbound_total.fetch_add(
                1, std::memory_order_relaxed);
        }
    };

    auto distribute_message = [&](const OutgoingMessage& msg) {
        utils::metrics::counters().registry_copy_eliminated_total.fetch_add(
            1, std::memory_order_relaxed);
        if (msg.target.conns.size() > 1 &&
            std::holds_alternative<SendPayload>(msg.action)) {
            utils::metrics::counters().fanout_payload_shared_total.fetch_add(
                1, std::memory_order_relaxed);
        }
        for (const auto& global_id : msg.target.conns) {
            enqueue_to_connection(global_id, msg);
        }
    };

    while (processed < cfg_.max_per_tick) {
        if (cfg_.time_budget.count() > 0) {
            const auto now = std::chrono::steady_clock::now();
            if (now - start >= cfg_.time_budget) {
                break;
            }
        }

        OutgoingMessage msg;
        if (!out_q_.try_pop(msg)) {
            break;
        }

        distribute_message(msg);
        ++processed;
    }

    const auto conn_ids = conns_.get_ids();
    for (const auto& conn_id : conn_ids) {
        if (cfg_.time_budget.count() > 0) {
            const auto now = std::chrono::steady_clock::now();
            if (now - start >= cfg_.time_budget) {
                break;
            }
        }

        auto view = conns_.get_view(conn_id);
        if (!view.has_value()) {
            continue;
        }
        utils::metrics::counters().registry_view_access_total.fetch_add(
            1, std::memory_order_relaxed);

        OutgoingMessage msg;
        bool has_msg = false;
        auto pop_result = conns_.mutate(conn_id, [&](auto& ctx) {
            if (ctx.outbox.q.empty()) {
                return;
            }
            msg = std::move(ctx.outbox.q.front());
            ctx.outbox.q.pop_front();
            has_msg = true;
        });
        if (!pop_result.has_value() || !has_msg) {
            continue;
        }

        auto handle = view->handle;
        std::visit(
            [&](const auto& action) {
                using T = std::decay_t<decltype(action)>;
                if constexpr (std::is_same_v<T, SendPayload>) {
                    if (!handle.valid()) {
                        return;
                    }
                    const auto& bytes = *action.payload.data;
                    const auto status = handle.send(bytes, action.payload.is_binary);
                    if (status == transport::websocket::UwsSocket::SendStatus::SUCCESS) {
                        conns_.mutate(conn_id, [&](auto& ctx) { ctx.outbox.slow_hits = 0; });
                    } else if (status ==
                               transport::websocket::UwsSocket::SendStatus::BACKPRESSURE) {
                        utils::metrics::counters().outbound_backpressure_total.fetch_add(
                            1, std::memory_order_relaxed);
                        conns_.mutate(conn_id, [&](auto& ctx) {
                            ctx.outbox.q.push_front(std::move(msg));
                        });
                    } else if (transport::websocket::UwsSocket::SendStatus::DROPPED ==
                               status) {
                        utils::metrics::counters().dropped_outbound_total.fetch_add(
                            1, std::memory_order_relaxed);
                    }
                } else if constexpr (std::is_same_v<T, UpdateAuthState>) {
                    conns_.mutate(conn_id, [&](auto& ctx) {
                        ctx.auth.is_authenticated = action.is_authenticated;
                        ctx.auth.expires_at = action.expires_at;
                        ctx.outbox.slow_hits = 0;
                    });
                } else if constexpr (std::is_same_v<T, DropConnection>) {
                    if (handle.valid()) {
                        handle.end(action.code, action.reason);
                    }
                    conns_.mutate(conn_id, [&](auto& ctx) {
                        ctx.outbox.q.clear();
                        ctx.outbox.slow_hits = 0;
                    });
                }
            },
            msg.action);
    }

    utils::metrics::observe_outbound_msgs_per_tick(processed);
    utils::metrics::maybe_log();
}

}  // namespace net::outbound
