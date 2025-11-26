#include "app/worker/WorkerPool.h"

#include <iostream>
#include <variant>

namespace app {

WorkerPool::WorkerPool(EventQueue& in_queue, OutgoingQueue& out_queue, Dispatcher& dispatcher,
                       WorkerPoolConfig config)
    : in_queue_(in_queue), out_queue_(out_queue), dispatcher_(dispatcher), config_(config) {}

WorkerPool::~WorkerPool() { stop(); }

void WorkerPool::start() {
    if (running_) return;
    running_ = true;
    paused_ = false;

    workers_.reserve(config_.worker_count);
    for (std::size_t i = 0; i < config_.worker_count; ++i) {
        workers_.emplace_back([this, i] { worker_loop(i); });
    }
}

void WorkerPool::stop() {
    if (!running_) return;

    running_ = false;
    paused_ = false;

    // wake blocked workers on queue + pause gate
    in_queue_.stop();
    pause_cv_.notify_all();

    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }
    workers_.clear();
}

void WorkerPool::pause() {
    if (!running_) return;
    paused_ = true;
}

void WorkerPool::resume() {
    if (!running_) return;
    paused_ = false;
    pause_cv_.notify_all();
}

void WorkerPool::wait_if_paused() {
    if (!paused_) return;

    std::unique_lock<std::mutex> lk(pause_mtx_);
    pause_cv_.wait(lk, [this] { return !paused_.load() || !running_.load(); });
}

void WorkerPool::worker_loop(std::size_t worker_index) {
    Event evt;
    while (running_) {
        // NEW: pause gate before taking new work
        wait_if_paused();
        if (!running_) break;

        if (!in_queue_.pop(evt)) {
            // Queue stopped and empty → time to exit the worker loop
            break;
        }
        const EventPayload& payload = evt.payload;
        log(utils::LogLevel::WARN, "Worker ", worker_index, " picked up new event");

        // process evt
        std::visit(
            [&](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, CommandRequest>) {
                    const CommandRequest& req = arg;

                    // validate message
                    auto [valid, error_message, message, _] =
                        message_validator_.validate_message(req.payload);

                    if (!valid) {
                        send_error(req, "001", error_message);
                        return;
                    }

                    // prepare context
                    CommandContext ctx;
                    ctx.conn_id = req.conn_id;
                    ctx.user_id = req.user_id;
                    ctx.current_hub_id = req.current_hub_id;
                    ctx.current_channel_id = req.current_channel_id;
                    ctx.authenticated = req.authenticated;
                    ctx.input.data = message;
                    ctx.input.received_at = req.received_at;

                    // dispatch
                    dispatcher_.dispatch(ctx.input.data["type"], ctx);
                    if (!ctx.output.success) {
                        send_error(req, ctx.output.error_code, ctx.output.error_message);
                        return;
                    }

                    // send outgoing messages
                    for (auto& msg : ctx.output.messages) {
                        out_queue_.push(std::move(msg));
                    }

                    log(utils::LogLevel::WARN, "Worker ", worker_index,
                        " successfully processed command for conn_id: ", req.conn_id.value);

                } else if constexpr (std::is_same_v<T, DisconnectEvent>) {
                    // handle disconnect
                    const DisconnectEvent& devt = arg;
                    // For now, do nothing on disconnect
                    (void)devt;
                }
            },
            payload);
    }
}

void WorkerPool::send_error(const CommandRequest& req, std::string_view code,
                            std::string_view message) {
    nlohmann::json err = {{"type", "error"}, {"code", code}, {"message", message}};

    DirectMessage out;
    out.conn_id = req.conn_id;
    out.payload = err.dump();
    out_queue_.push(std::move(out));
}

}  // namespace app