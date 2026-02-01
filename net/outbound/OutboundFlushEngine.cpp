#include "net/outbound/OutboundFlushEngine.h"

#include "net/connection/ConnectionRegistery.h"
#include "net/transport/IOutboundTransport.h"
#include "net/transport/websocket/UwsTypes.h"
#include "utils/Metrics.h"

namespace net::outbound {

OutboundFlushEngine::OutboundFlushEngine(connection::ConnectionRegistery& registry,
                                         transport::IOutboundTransport& transport,
                                         std::chrono::milliseconds tick)
    : registry_(registry), transport_(transport), tick_(tick) {}

void OutboundFlushEngine::start() {
    if (timer_) return;
    auto* loop = uWS::Loop::get();
    if (!loop) return;
    loop_ = loop;
    timer_ = us_create_timer(reinterpret_cast<us_loop_t*>(loop), 0, sizeof(OutboundFlushEngine*));
    if (!timer_) return;
    auto** slot = reinterpret_cast<OutboundFlushEngine**>(us_timer_ext(timer_));
    if (slot) *slot = this;
    us_timer_set(timer_, &OutboundFlushEngine::on_timer, static_cast<int>(tick_.count()),
                 static_cast<int>(tick_.count()));
}

void OutboundFlushEngine::stop() {
    auto* timer = timer_;
    if (!timer) return;
    timer_ = nullptr;
    auto* loop = loop_;
    if (loop) {
        loop->defer([timer]() {
            if (!timer) return;
            auto** slot = reinterpret_cast<OutboundFlushEngine**>(us_timer_ext(timer));
            if (slot) *slot = nullptr;
            us_timer_set(timer, nullptr, 0, 0);
            us_timer_close(timer);
        });
    } else {
        auto** slot = reinterpret_cast<OutboundFlushEngine**>(us_timer_ext(timer));
        if (slot) *slot = nullptr;
        us_timer_set(timer, nullptr, 0, 0);
        us_timer_close(timer);
    }
    loop_ = nullptr;
}

void OutboundFlushEngine::on_timer(us_timer_t* timer) {
    auto** slot = reinterpret_cast<OutboundFlushEngine**>(us_timer_ext(timer));
    OutboundFlushEngine* self = slot ? *slot : nullptr;
    if (self) {
        self->on_tick();
    }
}

void OutboundFlushEngine::on_tick() {
    const auto conn_ids = registry_.get_ids();
    for (const auto& conn_id : conn_ids) {
        auto ready = registry_.take_one_outbound(conn_id);
        if (!ready.has_value()) {
            utils::metrics::counters().outbound_flush_empty_total.fetch_add(
                1, std::memory_order_relaxed);
            continue;
        }

        utils::metrics::counters().outbound_flush_total.fetch_add(1,
                                                                  std::memory_order_relaxed);

        if (ready->drop_pending) {
            if (ready->handle.valid()) {
                ready->handle.end(1013, "slow consumer");
            }
            utils::metrics::counters().slow_connection_dropped_total.fetch_add(
                1, std::memory_order_relaxed);
            continue;
        }

        auto& msg = ready->msg;
        auto handle = ready->handle;
        if (transport_.is_backpressured(handle)) {
            utils::metrics::counters().outbound_backpressured_total.fetch_add(
                1, std::memory_order_relaxed);
            registry_.mutate(conn_id, [&](auto& ctx) {
                ctx.outbox.q.push_front(std::move(msg));
            });
            continue;
        }
        std::visit(
            [&](const auto& action) {
                using T = std::decay_t<decltype(action)>;
                if constexpr (std::is_same_v<T, SendPayload>) {
                    if (!handle.valid()) {
                        utils::metrics::counters().outbound_flush_send_fail_total.fetch_add(
                            1, std::memory_order_relaxed);
                        return;
                    }
                    const auto& bytes = *action.payload.data;
                    const bool ok = transport_.send(handle, bytes, action.payload.is_binary);
                    if (ok) {
                        registry_.mutate(conn_id,
                                         [&](auto& ctx) { ctx.outbox.slow_hits = 0; });
                    } else {
                        utils::metrics::counters().outbound_flush_send_fail_total.fetch_add(
                            1, std::memory_order_relaxed);
                        bool drop_now = false;
                        registry_.mutate(conn_id, [&](auto& ctx) {
                            ctx.outbox.slow_hits += 1;
                            if (ctx.outbox.slow_hits >= 4) {
                                ctx.outbox.drop_pending = false;
                                ctx.outbox.slow_hits = 0;
                                ctx.outbox.q.clear();
                                drop_now = true;
                            } else {
                                ctx.outbox.q.push_front(std::move(msg));
                            }
                        });
                        if (drop_now && handle.valid()) {
                            handle.end(1013, "slow consumer");
                            utils::metrics::counters().slow_connection_dropped_total.fetch_add(
                                1, std::memory_order_relaxed);
                        }
                    }
                } else if constexpr (std::is_same_v<T, UpdateAuthState>) {
                    registry_.mutate(conn_id, [&](auto& ctx) {
                        ctx.auth.is_authenticated = action.is_authenticated;
                        ctx.auth.expires_at = action.expires_at;
                        ctx.outbox.slow_hits = 0;
                    });
                } else if constexpr (std::is_same_v<T, DropConnection>) {
                    if (handle.valid()) {
                        handle.end(action.code, action.reason);
                    }
                    registry_.mutate(conn_id, [&](auto& ctx) {
                        ctx.outbox.q.clear();
                        ctx.outbox.slow_hits = 0;
                    });
                }
            },
            msg.action);
    }
}

}  // namespace net::outbound
