#include "net/outbound/OutgoingWorker.h"
#include "utils/Metrics.h"

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

    auto process_message = [&](const OutgoingMessage& msg) {
        utils::metrics::counters().registry_copy_eliminated_total.fetch_add(
            1, std::memory_order_relaxed);
        for (const auto& global_id : msg.target.conns) {
            auto view = conns_.get_view(global_id.conn_id);
            if (!view.has_value()) {
                utils::metrics::counters().registry_miss_total.fetch_add(
                    1, std::memory_order_relaxed);
                utils::metrics::counters().dropped_outbound_total.fetch_add(
                    1, std::memory_order_relaxed);
                continue;
            }
            utils::metrics::counters().registry_view_access_total.fetch_add(
                1, std::memory_order_relaxed);

            const auto conn_id = view->conn_id;
            auto handle = view->handle;

            // Process action
            std::visit(
                [&](const auto& action) {
                    using T = std::decay_t<decltype(action)>;
                    if constexpr (std::is_same_v<T, SendPayload>) {
                        if (!handle.valid()) return;
                        const auto status =
                            handle.send(action.payload.data, action.payload.is_binary);
                        if (status == transport::websocket::UwsSocket::SendStatus::SUCCESS) {
                            // hot-path: avoid per-message logging
                        } else if (status ==
                                   transport::websocket::UwsSocket::SendStatus::BACKPRESSURE) {
                            utils::metrics::counters().outbound_backpressure_total.fetch_add(
                                1, std::memory_order_relaxed);
#if defined(SERCOM_DEBUG_LOGS)
                            log(utils::LogLevel::WARN, "Backpressure on connection: ",
                                conn_id.value);
#endif
                            conns_.mutate(conn_id, [&](auto& ctx) {
                                ctx.pending.push_back(std::make_pair(
                                    action.payload.data,
                                    action.payload.is_binary ? uWS::OpCode::BINARY
                                                             : uWS::OpCode::TEXT));
                            });
                        } else if (transport::websocket::UwsSocket::SendStatus::DROPPED ==
                                   status) {
                            utils::metrics::counters().dropped_outbound_total.fetch_add(
                                1, std::memory_order_relaxed);
#if defined(SERCOM_DEBUG_LOGS)
                            log(utils::LogLevel::ERROR,
                                "Connection closed while sending to: ", conn_id.value);
#endif
                        }
                    } else if constexpr (std::is_same_v<T, UpdateAuthState>) {
                        auto result = conns_.mutate(conn_id, [&](auto& ctx) {
                            ctx.auth.is_authenticated = action.is_authenticated;
                            ctx.auth.expires_at = action.expires_at;
                        });
                        if (!result.has_value()) {
                            utils::metrics::counters().dropped_outbound_total.fetch_add(
                                1, std::memory_order_relaxed);
#if defined(SERCOM_DEBUG_LOGS)
                            log(utils::LogLevel::ERROR,
                                "Failed to update auth state for connection: ", conn_id.value);
#endif
                        }
                    } else if constexpr (std::is_same_v<T, DropConnection>) {
                        if (!handle.valid()) return;
                        handle.end(action.code, action.reason);
#if defined(SERCOM_DEBUG_LOGS)
                        log(utils::LogLevel::INFO, "Dropped connection: ", conn_id.value,
                            " Reason: ", action.reason);
#endif
                    }
                },
                msg.action);
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

        process_message(msg);
        ++processed;
    }

    utils::metrics::observe_outbound_msgs_per_tick(processed);
    utils::metrics::maybe_log();
}

}  // namespace net::outbound
