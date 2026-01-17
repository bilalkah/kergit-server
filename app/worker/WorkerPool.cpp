#include "app/worker/WorkerPool.h"

#include "proto/event/error.pb.h"

#include <chrono>
#include <iostream>
#include <thread>
#include <variant>

using namespace sercom::protocol;
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
        // For worker
        wait_if_paused();
        if (!running_) break;
        auto evt = in_queue_.pop();
        if (!evt.has_value()) {
            log(utils::LogLevel::WARN, "Worker ", worker_index, " stopping: ", evt.error());
            break;
        }
        // For worker

        app::queue::Event event = std::move(evt.value());

        std::vector<net::outbound::OutgoingMessage> intents = std::visit(
            [&](auto&& event) -> std::vector<net::outbound::OutgoingMessage> {
                return handle_event(event);
            },
            event);

        for (const auto& intent : intents) {
            out_queue_.push(intent);
        }
    }
}

std::string WorkerPool::prepare_error_msg(const GlobalConnId& req, Envelope::Type type,
                                          event::CommandErrorCode code, std::string_view message) {
    Envelope env;
    env.set_version(1);
    env.set_type(Envelope::CommandError);

    event::CommandError err;
    err.set_command_type(type);
    err.set_code(code);

    if (!message.empty()) {
        err.set_message(message.data(), message.size());
    }

    err.SerializeToString(env.mutable_payload());
    return env.SerializeAsString();
}

bool WorkerPool::try_mark_executing(GlobalConnId conn, sercom::protocol::Envelope::Type type) {
    std::lock_guard<std::mutex> lock(executing_commands_mtx_);
    auto& set = executing_commands_[conn];
    if (set.contains(type)) {
        return false;  // duplicate in-flight → drop
    }
    set.insert(type);
    return true;
}

void WorkerPool::unmark_executing(GlobalConnId conn, sercom::protocol::Envelope::Type type) {
    std::lock_guard<std::mutex> lock(executing_commands_mtx_);
    auto it = executing_commands_.find(conn);
    if (it == executing_commands_.end()) return;

    it->second.erase(type);
    if (it->second.empty()) {
        executing_commands_.erase(it);
    }
}

std::vector<net::outbound::OutgoingMessage> WorkerPool::handle_event(
    const queue::MessageEvent& msg_evt) {
    std::vector<net::outbound::OutgoingMessage> result;
    const auto& env = msg_evt.payload.env;

    auto env_validation = proto_validator_.validate_envelope(env);
    if (!env_validation.has_value()) {
        // Drop connection on invalid envelope
        result.emplace_back(net::outbound::OutgoingMessage{
            .target = net::outbound::Target::one(msg_evt.conn_id),
            .action = net::outbound::DropConnection{
                .code = static_cast<int>(
                    sercom::protocol::event::CommandErrorCode::CommandErrorCode_INVALID_FORMAT),
                .reason = "Invalid envelope: " + env_validation.error(),
            }});
        return result;
    }

    if (!try_mark_executing(msg_evt.conn_id, env.type())) {
        log(utils::LogLevel::WARN, "Dropping duplicate in-flight command of type ",
            static_cast<int>(env.type()), " for connection ", msg_evt.conn_id.netstack_id.value,
            "/", msg_evt.conn_id.conn_id.value);
        return result;
    }

    result = dispatcher_.dispatch(env.type(), cmd_ctx_, msg_evt);

    unmark_executing(msg_evt.conn_id, env.type());
    return result;
}

std::vector<net::outbound::OutgoingMessage> WorkerPool::handle_event(
    const queue::ConnectionEvent& evt) {
    return dispatcher_.dispatch("connection", cmd_ctx_, evt);
}

std::vector<net::outbound::OutgoingMessage> WorkerPool::handle_event(
    const queue::DisconnectionEvent& evt) {
    return dispatcher_.dispatch("disconnection", cmd_ctx_, evt);
}

}  // namespace app::worker
