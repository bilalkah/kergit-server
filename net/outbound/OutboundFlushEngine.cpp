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
    (void)transport_;
    const auto conn_ids = registry_.get_ids();
    for (const auto& conn_id : conn_ids) {
        bool handled_action = false;
        bool drop_for_slow_consumer = false;
        bool close_connection = false;
        int close_code = 0;
        std::string close_reason;
        transport::WsHandle handle{};

        auto result = registry_.mutate(conn_id, [&](auto& ctx) {
            handle = ctx.handle;
            if (ctx.outbox.drop_pending) {
                ctx.outbox.drop_pending = false;
                ctx.outbox.slow_hits = 0;
                ctx.outbox.q.clear();
                handled_action = true;
                drop_for_slow_consumer = true;
                return;
            }

            if (ctx.outbox.q.empty()) {
                return;
            }

            auto& msg = ctx.outbox.q.front();
            std::visit(
                [&](const auto& action) {
                    using T = std::decay_t<decltype(action)>;
                    if constexpr (std::is_same_v<T, SendPayload>) {
                        // SendPayload flush is owned by OutgoingWorker.
                        return;
                    } else if constexpr (std::is_same_v<T, UpdateAuthState>) {
                        ctx.auth.is_authenticated = action.is_authenticated;
                        ctx.auth.expires_at = action.expires_at;
                        ctx.outbox.slow_hits = 0;
                        ctx.outbox.q.pop_front();
                        handled_action = true;
                    } else if constexpr (std::is_same_v<T, DropConnection>) {
                        close_code = action.code;
                        close_reason = action.reason;
                        ctx.outbox.q.pop_front();
                        ctx.outbox.q.clear();
                        ctx.outbox.slow_hits = 0;
                        handled_action = true;
                        close_connection = true;
                    }
                },
                msg.action);
        });

        if (!result.has_value() || !handled_action) {
            utils::metrics::counters().outbound_flush_empty_total.fetch_add(
                1, std::memory_order_relaxed);
            continue;
        }

        utils::metrics::counters().outbound_flush_total.fetch_add(1,
                                                                  std::memory_order_relaxed);

        if (drop_for_slow_consumer && handle.valid()) {
            handle.end(1013, "slow consumer");
            utils::metrics::counters().slow_connection_dropped_total.fetch_add(
                1, std::memory_order_relaxed);
        }

        if (close_connection && handle.valid()) {
            handle.end(close_code, close_reason);
        }
    }
}

}  // namespace net::outbound
