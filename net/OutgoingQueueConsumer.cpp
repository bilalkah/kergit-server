#include "net/OutgoingQueueConsumer.h"

#include <nlohmann/json.hpp>

using nlohmann::json;

namespace net {
OutgoingQueueConsumer::OutgoingQueueConsumer(core::IApp& app, ConnectionManager& conns,
                                             ClientGateway& gateway, OutgoingQueue& out_q,
                                             OutgoingConsumerConfig cfg)
    : app_(app), conns_(conns), cli_gtw_(gateway), out_q_(out_q), cfg_(std::move(cfg)) {}

OutgoingQueueConsumer::~OutgoingQueueConsumer() { stop(); }

void OutgoingQueueConsumer::start() {
    if (running_.exchange(true)) return;
    auto* loop = app_.uws().getLoop();
    if (!loop) {
        running_.store(false);
        return;
    }
    timer_ = us_create_timer(reinterpret_cast<us_loop_t*>(loop), 0, sizeof(OutgoingQueueConsumer*));
    if (!timer_) {
        running_.store(false);
        return;
    }
    auto** slot = reinterpret_cast<OutgoingQueueConsumer**>(us_timer_ext(timer_));
    if (slot) *slot = this;
    us_timer_set(timer_, &OutgoingQueueConsumer::on_timer, static_cast<int>(cfg_.interval.count()),
                 static_cast<int>(cfg_.interval.count()));
}

void OutgoingQueueConsumer::stop() {
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
            auto** slot = reinterpret_cast<OutgoingQueueConsumer**>(us_timer_ext(timer));
            if (slot) *slot = nullptr;
            us_timer_set(timer, nullptr, 0, 0);
            us_timer_close(timer);
        });
    } else {
        auto** slot = reinterpret_cast<OutgoingQueueConsumer**>(us_timer_ext(timer));
        if (slot) *slot = nullptr;
        us_timer_set(timer, nullptr, 0, 0);
        us_timer_close(timer);
    }
}

void OutgoingQueueConsumer::on_timer(us_timer_t* timer) {
    auto** slot = reinterpret_cast<OutgoingQueueConsumer**>(us_timer_ext(timer));
    OutgoingQueueConsumer* self = slot ? *slot : nullptr;
    if (self) {
        self->tick();
    }
}

void OutgoingQueueConsumer::tick() {
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
                UwsSocket* ws = conns_.get(arg.conn_id);
                if (ws) {
                    auto* psd = ws->getUserData();
                    if (arg.apply_psd) {
                        arg.apply_psd(psd);
                    }
                    cli_gtw_.send_defer(arg.conn_id, json::parse(arg.payload));
                }
            } else if constexpr (std::is_same_v<T, PublishMessage>) {
                cli_gtw_.publish(arg.topic, arg.payload);
            }
        },
        msg);
}

}  // namespace net
