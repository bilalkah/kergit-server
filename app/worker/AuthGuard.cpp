#include "app/worker/AuthGuard.h"

namespace app::worker {

AuthGuard::AuthGuard(SessionManager& sessions, net::outbound::IOutboundSink& out_sink,
                     std::chrono::seconds grace)
    : sessions_(sessions), out_sink_(out_sink), default_grace_(grace) {}

AuthGuard::~AuthGuard() { stop(); }

void AuthGuard::start() {
    stop_flag_ = false;
    thread_ = std::jthread([this] { loop(); });
}

void AuthGuard::stop() {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        stop_flag_ = true;
    }
    cv_.notify_all();
    if (thread_.joinable()) thread_.join();
}

void AuthGuard::schedule(const GlobalConnId& conn, std::chrono::seconds grace_override) {
    std::lock_guard<std::mutex> lk(mtx_);
    auto at = std::chrono::steady_clock::now() +
              (grace_override.count() > 0 ? grace_override : default_grace_);
    deadlines_.push(AuthDeadline{at, conn});
    cv_.notify_all();
}

void AuthGuard::loop() {
    std::unique_lock<std::mutex> lk(mtx_);
    while (true) {
        if (stop_flag_) break;
        if (deadlines_.empty()) {
            cv_.wait(lk, [this] { return stop_flag_ || !deadlines_.empty(); });
            continue;
        }

        auto next = deadlines_.top();
        auto now = std::chrono::steady_clock::now();
        if (now < next.at) {
            cv_.wait_until(lk, next.at);
            continue;
        }

        deadlines_.pop();
        lk.unlock();

        if (!sessions_.sessionOfConnection(next.conn).has_value()) {
            net::outbound::OutgoingMessage drop;
            drop.target = net::outbound::Target::one(next.conn);
            drop.action = net::outbound::DropConnection{.code = 4401, .reason = "auth_timeout"};
            out_sink_.push(std::move(drop));
        }

        lk.lock();
    }
}

}  // namespace app::worker
