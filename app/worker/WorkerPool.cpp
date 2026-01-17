#include "app/worker/WorkerPool.h"

#include "proto/envelope.pb.h"
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
        Envelope env;

        // process evt
        std::visit(
            [&](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, app::queue::MessageEvent>) {
                    const app::queue::MessageEvent& msg_evt = arg;

                    if (!env.ParseFromString(msg_evt.payload.data)) {
                        send_error(event.conn_id, Envelope::UNKNOWN,
                                   event::CommandErrorCode_INVALID_FORMAT,
                                   "Invalid envelope format");
                        return;
                    }

                    // validate envelope
                    auto env_validation = proto_validator_.validate_envelope(env);
                    if (!env_validation.has_value()) {
                        send_error(event.conn_id, Envelope::UNKNOWN,
                                   event::CommandErrorCode_INVALID_ARGUMENT,
                                   env_validation.error());
                        return;
                    }

                    {
                        // check for duplicate executing command for this connection
                        std::lock_guard<std::mutex> lock(executing_commands_mtx_);
                        if (executing_commands_.find(event.conn_id) != executing_commands_.end()) {
                            // duplicate found
                            send_error(event.conn_id, env.type(),
                                       event::CommandErrorCode_DUPLICATE_COMMAND,
                                       "Another command is currently being processed for "
                                       "this connection");
                            return;
                        }
                        executing_commands_[event.conn_id] = env.type();
                    }

                    // parse payload based on type
                    MessageEvent input{
                        .conn = event.conn_id,
                        .body = env.payload(),
                    };

                    // dispatch
                    cmd_result = dispatcher_.dispatch(env.type(), cmd_ctx_, input);

                    {
                        // remove from executing commands cache
                        std::lock_guard<std::mutex> lock(executing_commands_mtx_);
                        executing_commands_.erase(event.conn_id);
                    }

                } else if constexpr (std::is_same_v<T, app::queue::ConnectionEvent>) {
                    const ConnectEvent cmd{
                        .conn = event.conn_id,
                        .user_id = arg.user_id,
                    };
                    cmd_result = dispatcher_.dispatch("connection", cmd_ctx_, cmd);

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
            send_error(event.conn_id, env.type(),
                       event::CommandErrorCode::CommandErrorCode_UNSPECIFIED, err.message);
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
                                .payload = net::outbound::Payload{.data = arg.payload}};
                        } else if constexpr (std::is_same_v<T, BinaryUnicast>) {
                            out_msg.target = net::outbound::Target::one(arg.conn);
                            out_msg.action = net::outbound::SendPayload{
                                .payload = net::outbound::Payload{.data = arg.payload,
                                                                  .is_binary = true}};
                        } else if constexpr (std::is_same_v<T, Fanout>) {
                            out_msg.target = net::outbound::Target::many(arg.conns);
                            out_msg.action = net::outbound::SendPayload{
                                .payload = net::outbound::Payload{.data = arg.payload}};
                        } else if constexpr (std::is_same_v<T, BinaryFanout>) {
                            out_msg.target = net::outbound::Target::many(arg.conns);
                            out_msg.action = net::outbound::SendPayload{
                                .payload = net::outbound::Payload{.data = arg.payload,
                                                                  .is_binary = true}};

                        } else if constexpr (std::is_same_v<T, AuthStateIntent>) {
                            out_msg.target = net::outbound::Target::one(arg.conn);
                            out_msg.action = net::outbound::UpdateAuthState{
                                .is_authenticated = arg.authenticated,
                                .expires_at = arg.expires_at};
                        } else if constexpr (std::is_same_v<T, DropConnectionIntent>) {
                            out_msg.target = net::outbound::Target::one(arg.conn);
                            out_msg.action = net::outbound::DropConnection{.code = arg.code,
                                                                           .reason = arg.reason};
                        }
                        out_queue_.push(std::move(out_msg));
                    },
                    intent);
            }
        }
    }
}

void WorkerPool::send_error(const GlobalConnId& req, Envelope::Type type,
                            event::CommandErrorCode code, std::string_view message) {
    // construct error envelope
    Envelope env;
    env.set_version(1);
    env.set_type(Envelope::CommandError);
    event::CommandError err;
    err.set_command_type(type);
    err.set_code(code);
    err.set_message(std::string(message));
    std::string* payload = env.mutable_payload();
    err.SerializeToString(payload);
    // send
    net::outbound::OutgoingMessage out_msg;
    out_msg.target = net::outbound::Target::one(req);
    out_msg.action = net::outbound::SendPayload{
        .payload = net::outbound::Payload{.data = env.SerializeAsString(), .is_binary = true}};
    out_queue_.push(std::move(out_msg));
}

}  // namespace app::worker
