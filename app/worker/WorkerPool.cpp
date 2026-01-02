#include "app/worker/WorkerPool.h"

#include <iostream>
#include <variant>

namespace app::worker {

WorkerPool::WorkerPool(queue::EventQueue& in_queue, net::outbound::IOutboundSink& out_queue,
                       Dispatcher& dispatcher, CommandContext& cmd_ctx,
                       core::AppStackConfig appstack_config)
    : in_queue_(in_queue),
      out_queue_(out_queue),
      dispatcher_(dispatcher),
      cmd_ctx_(cmd_ctx),
      message_validator_(dispatcher.registered_commands()),
      config_(appstack_config) {}

WorkerPool::~WorkerPool() { stop(); }

void WorkerPool::start() {
    if (running_) return;
    running_ = true;
    paused_ = false;

    workers_.reserve(config_.worker_threads);
    for (std::size_t i = 0; i < config_.worker_threads; ++i) {
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
        t.request_stop();
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
    log(utils::LogLevel::INFO, "Worker ", worker_index, " started.");
    while (running_) {
        // NEW: pause gate before taking new work
        wait_if_paused();
        if (!running_) break;

        auto evt = in_queue_.pop();
        if (!evt.has_value()) {
            log(utils::LogLevel::WARN, "Worker ", worker_index, " stopping: ", evt.error());
            break;
        }

        app::queue::Event event = evt.value();
        CommandResult cmd_result;

        // process evt
        std::visit(
            [&](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, app::queue::MessageEvent>) {
                    const app::queue::MessageEvent& msg_evt = arg;

                    // validate message
                    auto msg_expected = message_validator_.validate_message(msg_evt.payload.data);

                    if (!msg_expected.has_value()) {
                        send_error(event.conn_id, "0000", msg_expected.error());
                        return;
                    }

                    auto message = msg_expected.value().message;

                    {
                        // check for duplicate command processing
                        std::lock_guard<std::mutex> lock(executing_commands_mtx_);
                        auto it = executing_commands_.find(event.conn_id);
                        if (it != executing_commands_.end()) {
                            if (executing_commands_[event.conn_id] != "send_message") {
                                log(utils::LogLevel::WARN, "Worker ", worker_index,
                                    " skipping duplicate command for net: ",
                                    event.conn_id.netstack_id.value,
                                    " conn: ", event.conn_id.conn_id.value);
                                return;
                            }
                        }
                        executing_commands_[event.conn_id] = message["type"];
                    }

                    JsonInput input;
                    input.conn = event.conn_id;
                    input.body = message;

                    // dispatch
                    cmd_result = dispatcher_.dispatch(message["type"], cmd_ctx_, input);

                } else if constexpr (std::is_same_v<T, app::queue::ConnectionEvent>) {
                    net::outbound::OutgoingMessage welcome_msg;
                    nlohmann::json welcome_payload = {{"type", "welcome"},
                                                      {"message", "Connection established"}};
                    welcome_msg.target = net::outbound::Target::one(event.conn_id);
                    welcome_msg.action = net::outbound::SendPayload{
                        .payload = net::outbound::Payload{welcome_payload.dump()}};

                    cmd_result = CommandSuccess{};
                    cmd_result->intents.push_back(
                        Unicast{.conn = event.conn_id, .payload = welcome_payload});

                } else if constexpr (std::is_same_v<T, app::queue::DisconnectionEvent>) {
                    // handle disconnect
                    const app::queue::DisconnectionEvent& devt = arg;
                    const DisconnectEvent cmd{
                        .conn = event.conn_id,
                        .code = devt.code,
                        .reason = devt.reason,
                    };
                    cmd_result = dispatcher_.dispatch("disconnection", cmd_ctx_, cmd);
                }
            },
            event.body);

        // handle command result
        if (!cmd_result.has_value()) {
            const CommandError& err = cmd_result.error();
            send_error(event.conn_id, err.code, err.message);
        } else {
            const CommandSuccess& success = cmd_result.value();
            for (const auto& intent : success.intents) {
                std::visit(
                    [&](auto&& arg) {
                        using T = std::decay_t<decltype(arg)>;
                        net::outbound::OutgoingMessage out_msg;
                        if constexpr (std::is_same_v<T, Unicast>) {
                            out_msg.target = net::outbound::Target::one(arg.conn);
                            out_msg.action = net::outbound::SendPayload{
                                .payload = net::outbound::Payload{arg.payload.dump()}};
                        } else if constexpr (std::is_same_v<T, Fanout>) {
                            out_msg.target = net::outbound::Target::many(arg.conns);
                            out_msg.action = net::outbound::SendPayload{
                                .payload = net::outbound::Payload{arg.payload.dump()}};
                        }
                        out_queue_.push(std::move(out_msg));
                    },
                    intent);
            }
        }

        {
            // remove from executing commands cache
            std::lock_guard<std::mutex> lock(executing_commands_mtx_);
            executing_commands_.erase(event.conn_id);
        }
    }
}

void WorkerPool::send_error(const GlobalConnId& req, std::string_view code,
                            std::string_view message) {
    nlohmann::json err = {{"type", "error"}, {"code", code}, {"message", message}};

    net::outbound::OutgoingMessage out_msg;
    out_msg.target = net::outbound::Target::one(req);
    out_msg.action = net::outbound::SendPayload{.payload = net::outbound::Payload{err.dump()}};
    out_queue_.push(std::move(out_msg));
}

}  // namespace app::worker