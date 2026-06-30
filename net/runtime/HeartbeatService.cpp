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
        c.heartbeat.connected_at = now;
        c.heartbeat.last_seen_at = now;
    });
}

void HeartbeatService::note_seen(const ConnId& conn_id) {
    const auto now = std::chrono::system_clock::now();
    conns_.mutate(conn_id,
                  [&](net::connection::ConnectionContext& c) { c.heartbeat.last_seen_at = now; });
}

void HeartbeatService::on_timer(us_timer_t* timer) {
    auto** slot = reinterpret_cast<HeartbeatService**>(us_timer_ext(timer));
    HeartbeatService* self = slot ? *slot : nullptr;
    if (self) self->tick();
}

/**
 * Heartbeat sweep: a passive liveness/lifecycle reaper. It never sends WebSocket
 * control-frame pings (those can be answered by an intermediary proxy and would be a
 * false liveness signal). Liveness comes from the client's app-level PING via
 * note_seen(); here we only close connections that have:
 *   - failed/expired auth, or
 *   - gone silent (no app PING) for longer than the timeout.
 */
void HeartbeatService::tick() {
    if (!running_.load()) return;
    const auto now = std::chrono::system_clock::now();
    const auto conn_ids = conns_.get_ids();

    for (const auto& id : conn_ids) {
        // Hot path: read a no-copy view rather than a full ConnectionContext (which
        // would deep-copy the outbox/reliable/pending deques on every sweep). The
        // heartbeat timer and every registry mutation run on the same per-stack loop
        // thread, so this snapshot cannot change underneath us mid-iteration — no
        // re-read guard is needed before acting.
        auto view = conns_.get_view(id);
        if (!view.has_value()) continue;
        if (!view->handle.valid()) continue;

        if (view->auth_state == connection::AuthState::AUTH_FAILED) {
            view->handle.end(4401, "auth_failed");
            continue;
        }

        if (view->auth_state == connection::AuthState::AUTH_PENDING) {
            if (now - view->connected_at >= cfg_.auth_pending_timeout) {
                conns_.mutate(id, [&](net::connection::ConnectionContext& real) {
                    real.auth_state = connection::AuthState::AUTH_FAILED;
                });
                view->handle.end(4403, "auth_timeout");
            }
            continue;
        }

        // Only check token expiry for fully authenticated connections.
        if (view->auth_state == connection::AuthState::AUTHENTICATED &&
            now >= view->auth_expires_at) {
            conns_.mutate(id, [&](net::connection::ConnectionContext& real) {
                real.auth_state = connection::AuthState::AUTH_FAILED;
            });
            view->handle.end(4402, "auth_token_expired");
            continue;
        }

        // Liveness: time since the last app-level PING. A genuinely gone/frozen client
        // stops sending app PINGs (its JS no longer runs), so last_seen_at goes stale and
        // we reap it here. A backgrounded-but-alive tab keeps sending throttled pings,
        // which stay within the (generous) timeout, so it is kept.
        if (now - view->last_seen_at > cfg_.timeout) {
            view->handle.end(cfg_.close_code, cfg_.close_reason);
            continue;
        }
    }
}

}  // namespace net::runtime
