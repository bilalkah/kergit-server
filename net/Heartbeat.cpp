#include "net/Heartbeat.h"

#include <libusockets.h>
#include <nlohmann/json.hpp>

using nlohmann::json;

namespace net {

Heartbeat::Heartbeat(core::IApp& app, ConnectionManager& conns, HeartbeatConfig cfg)
    : app_(app), conns_(conns), cfg_(std::move(cfg)) {}

Heartbeat::~Heartbeat() { stop(); }

void Heartbeat::start() {
    if (running_.exchange(true)) return;
    auto* loop = app_.uws().getLoop();
    if (!loop) {
        running_.store(false);
        return;
    }
    timer_ = us_create_timer(reinterpret_cast<us_loop_t*>(loop), 0, sizeof(Heartbeat*));
    if (!timer_) {
        running_.store(false);
        return;
    }
    auto** slot = reinterpret_cast<Heartbeat**>(us_timer_ext(timer_));
    if (slot) *slot = this;
    us_timer_set(timer_, &Heartbeat::on_timer, static_cast<int>(cfg_.interval.count()),
                 static_cast<int>(cfg_.interval.count()));
}

void Heartbeat::stop() {
    auto* timer = timer_;
    if (!timer) {
        running_.store(false);
        return;
    }
    timer_ = nullptr;
    running_.store(false);

    if (auto* loop = app_.uws().getLoop()) {
        loop->defer([timer]() {
            if (!timer) return;
            auto** slot = reinterpret_cast<Heartbeat**>(us_timer_ext(timer));
            if (slot) *slot = nullptr;
            us_timer_set(timer, nullptr, 0, 0);
            us_timer_close(timer);
        });
    } else {
        auto** slot = reinterpret_cast<Heartbeat**>(us_timer_ext(timer));
        if (slot) *slot = nullptr;
        us_timer_set(timer, nullptr, 0, 0);
        us_timer_close(timer);
    }
}

void Heartbeat::on_open(PerSocketData& psd) { psd.last_pong_at = std::chrono::steady_clock::now(); }

void Heartbeat::on_pong(PerSocketData& psd) { psd.last_pong_at = std::chrono::steady_clock::now(); }

void Heartbeat::on_timer(us_timer_t* timer) {
    auto** slot = reinterpret_cast<Heartbeat**>(us_timer_ext(timer));
    Heartbeat* self = slot ? *slot : nullptr;
    if (self) self->tick();
}

void Heartbeat::tick() {
    const auto now = std::chrono::steady_clock::now();
    const auto epoch_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch());
    const std::string payload = json{{"type", "ping"}, {"ts", epoch_ms.count()}}.dump();

    auto sockets = conns_.get_all();
    for (auto* ws : sockets) {
        if (!ws) continue;
        auto* psd = ws->getUserData();
        if (!psd) continue;

        if (now - psd->last_pong_at > cfg_.timeout) {
            ws->end(cfg_.close_code, cfg_.close_reason);
            continue;
        }

        ws->send(payload, OpCode::TEXT);
    }
}

}  // namespace net
