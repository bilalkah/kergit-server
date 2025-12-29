#include "net/outbound/OutgoingWorker.h"

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

    log(utils::LogLevel::WARN, "Processing outgoing message from queue");
    const OutgoingMessage& msg = expected_msg.value();

    std::visit(
        [&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, DirectMessage>) {
                auto expected_context = conns_.get(arg.conn_id);
                if (expected_context.has_value()) {
                    auto context = expected_context.value();

                    std::visit(
                        [&](auto& handle) {
                            if (!handle.valid()) return;
                            handle.send(arg.payload.data);
                        },
                        context.handle);
                }
            } else if constexpr (std::is_same_v<T, PublishMessage>) {
                auto expected_contexts = conns_.get(arg.conn_ids);
                for (const auto& expected_context : expected_contexts) {
                    if (expected_context.has_value()) {
                        auto context = expected_context.value();
                        std::visit(
                            [&](auto& handle) {
                                if (!handle.valid()) return;
                                handle.send(arg.payload.data);
                            },
                            context.handle);
                    }
                }
            }
        },
        msg);
}

}  // namespace net::outbound
