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
    auto expected_msg = out_q_.try_pop();

    if (!expected_msg.has_value()) {
        // log(utils::LogLevel::WARN, expected_msg.error());
        return;
    }

    const OutgoingMessage& msg = expected_msg.value();

    auto conn_opt = conns_.get(msg.target.conns);
    for (auto& conn_res : conn_opt) {
        if (!conn_res.has_value()) {
            utils::metrics::counters().dropped_outbound_total.fetch_add(
                1, std::memory_order_relaxed);
#if defined(SERCOM_DEBUG_LOGS)
            log(utils::LogLevel::ERROR,
                "Connection not found for outgoing message: ", conn_res.error().message);
#endif
            continue;
        }

        auto& conn_ctx = conn_res.value();
        // Process action
        std::visit(
            [&](const auto& action) {
                using T = std::decay_t<decltype(action)>;
                if constexpr (std::is_same_v<T, SendPayload>) {
                    if (!conn_ctx.handle.valid()) return;
                    const auto status =
                        conn_ctx.handle.send(action.payload.data, action.payload.is_binary);
                    if (status == transport::websocket::UwsSocket::SendStatus::SUCCESS) {
                        // hot-path: avoid per-message logging
                    } else if (status ==
                               transport::websocket::UwsSocket::SendStatus::BACKPRESSURE) {
                        utils::metrics::counters().outbound_backpressure_total.fetch_add(
                            1, std::memory_order_relaxed);
#if defined(SERCOM_DEBUG_LOGS)
                        log(utils::LogLevel::WARN,
                            "Backpressure on connection: ", conn_ctx.conn_id.value);
#endif
                        conns_.mutate(conn_ctx.conn_id, [&](auto& ctx) {
                            ctx.pending.push_back(std::make_pair(
                                action.payload.data, action.payload.is_binary ? uWS::OpCode::BINARY
                                                                              : uWS::OpCode::TEXT));
                        });
                    } else if (transport::websocket::UwsSocket::SendStatus::DROPPED == status) {
                        utils::metrics::counters().dropped_outbound_total.fetch_add(
                            1, std::memory_order_relaxed);
#if defined(SERCOM_DEBUG_LOGS)
                        log(utils::LogLevel::ERROR,
                            "Connection closed while sending to: ", conn_ctx.conn_id.value);
#endif
                    }
                } else if constexpr (std::is_same_v<T, UpdateAuthState>) {
                    auto result = conns_.mutate(conn_ctx.conn_id, [&](auto& ctx) {
                        ctx.auth.is_authenticated = action.is_authenticated;
                        ctx.auth.expires_at = action.expires_at;
                    });
                    if (!result.has_value()) {
                        utils::metrics::counters().dropped_outbound_total.fetch_add(
                            1, std::memory_order_relaxed);
#if defined(SERCOM_DEBUG_LOGS)
                        log(utils::LogLevel::ERROR,
                            "Failed to update auth state for connection: ", conn_ctx.conn_id.value);
#endif
                    }
                } else if constexpr (std::is_same_v<T, DropConnection>) {
                    if (!conn_ctx.handle.valid()) return;
                    conn_ctx.handle.end(action.code, action.reason);
#if defined(SERCOM_DEBUG_LOGS)
                    log(utils::LogLevel::INFO, "Dropped connection: ", conn_ctx.conn_id.value,
                        " Reason: ", action.reason);
#endif
                }
            },
            msg.action);
    }

    utils::metrics::maybe_log();
}

}  // namespace net::outbound
