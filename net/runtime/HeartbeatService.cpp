#include "net/runtime/HeartbeatService.h"

#include "utils/Metrics.h"

namespace net::runtime {

HeartbeatService::HeartbeatService(net::transport::ILoop& loop,
                                   connection::ConnectionRegistery& conns, HeartbeatConfig cfg)
    : loop_(loop), conns_(conns), cfg_(std::move(cfg)) {}

HeartbeatService::~HeartbeatService() { stop(); }

void HeartbeatService::start() {
    if (running_.exchange(true)) return;
    auto* loop = loop_.getUwsLoop();
    if (!loop) {
        running_.store(false);
        return;
    }
    timer_ = us_create_timer(reinterpret_cast<us_loop_t*>(loop), 0, sizeof(HeartbeatService*));
    if (!timer_) {
        running_.store(false);
        return;
    }
    auto** slot = reinterpret_cast<HeartbeatService**>(us_timer_ext(timer_));
    if (slot) *slot = this;
    us_timer_set(timer_, &HeartbeatService::on_timer, static_cast<int>(cfg_.interval.count()),
                 static_cast<int>(cfg_.interval.count()));
}

void HeartbeatService::stop() {
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
            auto** slot = reinterpret_cast<HeartbeatService**>(us_timer_ext(timer));
            if (slot) *slot = nullptr;
            us_timer_set(timer, nullptr, 0, 0);
            us_timer_close(timer);
        });
    } else {
        auto** slot = reinterpret_cast<HeartbeatService**>(us_timer_ext(timer));
        if (slot) *slot = nullptr;
        us_timer_set(timer, nullptr, 0, 0);
        us_timer_close(timer);
    }
}

void HeartbeatService::on_open(ConnId conn_id) {
    conns_.mutate(conn_id, [&](net::connection::ConnectionContext& c) {
        auto now = std::chrono::system_clock::now();
        c.heartbeat.alive = true;
        c.heartbeat.connected_at = now;
        c.heartbeat.last_ping_at = now;
        c.heartbeat.last_pong_at = now;
        c.heartbeat.rtt_ms = std::chrono::milliseconds{0};
    });
}

std::expected<std::string, connection::ConnectionError> HeartbeatService::on_pong(ConnId conn_id) {
    const auto now = std::chrono::system_clock::now();

    conns_.mutate(conn_id, [&](net::connection::ConnectionContext& c) {
        c.heartbeat.alive = true;
        c.heartbeat.last_pong_at = now;
        c.heartbeat.rtt_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - c.heartbeat.last_ping_at);

        // Track client RTT in metrics
        utils::metrics::observe_client_rtt(static_cast<uint64_t>(c.heartbeat.rtt_ms.count()));
    });

    auto conn = conns_.get(conn_id);
    if (conn.has_value()) {
        auto context = conn.value();
        return "ok";
    }
    return std::unexpected(connection::ConnectionError{"Connection not found"});
}

void HeartbeatService::on_timer(us_timer_t* timer) {
    auto** slot = reinterpret_cast<HeartbeatService**>(us_timer_ext(timer));
    HeartbeatService* self = slot ? *slot : nullptr;
    if (self) self->tick();
}

/**
 * Heartbeat tick: ping all connections, check for timeouts
 * - close connections that have timed out
 * - send pings to alive connections
 * - update heartbeat state in connection registry
 */
void HeartbeatService::tick() {
    if (!running_.load()) return;
    const auto now = std::chrono::system_clock::now();
    const auto conn_ids = conns_.get_ids();

    for (const auto& id : conn_ids) {
        auto conn = conns_.get(id);
        if (!conn.has_value()) continue;
        auto ctx = conn.value();
        if (!ctx.handle.valid()) continue;

        if (ctx.auth_state == connection::AuthState::AUTH_FAILED) {
            auto latest = conns_.get_view(id);
            if (!latest.has_value() || latest->auth_state != connection::AuthState::AUTH_FAILED) {
                continue;
            }
            const char* reason = "auth_failed";
            ctx.handle.end(4401, reason);
            continue;
        }

        if (ctx.auth_state == connection::AuthState::AUTH_PENDING) {
            if (now - ctx.heartbeat.connected_at >= cfg_.auth_pending_timeout) {
                auto latest = conns_.get_view(id);
                if (!latest.has_value() ||
                    latest->auth_state != connection::AuthState::AUTH_PENDING) {
                    continue;
                }
                conns_.mutate(id, [&](net::connection::ConnectionContext& real) {
                    real.auth_state = connection::AuthState::AUTH_FAILED;
                });
                const char* reason = "auth_timeout";
                ctx.handle.end(4403, reason);
            }
            continue;
        }

        // Only check token expiry for fully authenticated connections.
        if (ctx.auth_state == connection::AuthState::AUTHENTICATED && now >= ctx.auth_expires_at) {
            auto latest = conns_.get_view(id);
            if (!latest.has_value() || latest->auth_state != connection::AuthState::AUTHENTICATED) {
                continue;
            }
            conns_.mutate(id, [&](net::connection::ConnectionContext& real) {
                real.auth_state = connection::AuthState::AUTH_FAILED;
            });
            const char* reason = "auth_token_expired";
            ctx.handle.end(4402, reason);
            continue;
        }

        if (ctx.heartbeat.rtt_ms > cfg_.timeout) {
            ctx.handle.end(cfg_.close_code, cfg_.close_reason);
            continue;
        }

        // update last_ping_at in real registry, then send ping
        conns_.mutate(id, [&](net::connection::ConnectionContext& real) {
            real.heartbeat.last_ping_at = now;
        });

        ctx.handle.ping();
    }
}

}  // namespace net::runtime
