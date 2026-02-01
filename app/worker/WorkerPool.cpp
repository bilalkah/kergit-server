#include "app/worker/WorkerPool.h"

#include "proto/event/error.pb.h"
#include "utils/Metrics.h"

#include <cassert>
#include <chrono>
#include <limits>
#include <thread>
#include <variant>

using namespace sercom::protocol;
namespace app::worker {
namespace {
thread_local std::size_t worker_index_tls = std::numeric_limits<std::size_t>::max();
}  // namespace

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
    worker_index_tls = worker_index;
    log(utils::LogLevel::INFO, "Worker ", worker_index, " started.");
    while (running_) {
        // For worker
        wait_if_paused();
        if (!running_) break;
        static thread_local uint32_t empty_polls = 0;
        app::queue::Event event;
        if (!in_queue_.try_pop(event)) {
            constexpr uint32_t kYieldSpins = 16;
            constexpr auto kSleepDuration = std::chrono::microseconds(100);
            if (empty_polls < kYieldSpins) {
                ++empty_polls;
                std::this_thread::yield();
            } else {
                std::this_thread::sleep_for(kSleepDuration);
            }
            continue;
        }
        empty_polls = 0;
        // For worker

        std::vector<net::outbound::OutgoingMessage> intents = std::visit(
            [&](auto&& event) -> std::vector<net::outbound::OutgoingMessage> {
                return handle_event(event);
            },
            event);

        for (const auto& intent : intents) {
            (void)out_queue_.push(std::move(intent));
            utils::metrics::counters().outbound_msgs_total.fetch_add(1,
                                                                     std::memory_order_relaxed);
        }

        utils::metrics::maybe_log();
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

std::vector<net::outbound::OutgoingMessage> WorkerPool::handle_event(queue::MessageEvent& msg_evt) {
    std::vector<net::outbound::OutgoingMessage> result;
    const auto& env = msg_evt.payload.env;

    utils::metrics::counters().payload_parse_total.fetch_add(1, std::memory_order_relaxed);
    auto parsed = proto_validator_.parse_and_validate(env);
    if (!parsed.has_value()) {
        utils::metrics::counters().payload_parse_fail_total.fetch_add(1,
                                                                      std::memory_order_relaxed);
        // Drop connection on invalid envelope
        result.emplace_back(net::outbound::OutgoingMessage{
            .target = net::outbound::Target::one(msg_evt.conn_id),
            .action =
                net::outbound::Action{std::in_place_type<net::outbound::DropConnection>,
                                      static_cast<int>(sercom::protocol::event::CommandErrorCode::
                                                           CommandErrorCode_INVALID_FORMAT),
                                      "Invalid envelope: " + parsed.error()}});
        return result;
    }
    msg_evt.payload.parsed = std::move(parsed.value());
    assert(!std::holds_alternative<std::monostate>(msg_evt.payload.parsed));

    if (!try_mark_executing(msg_evt.conn_id, env.type())) {
        utils::metrics::counters().dropped_inbound_total.fetch_add(1, std::memory_order_relaxed);
#if defined(SERCOM_DEBUG_LOGS)
        log(utils::LogLevel::WARN, "Dropping duplicate in-flight command of type ",
            static_cast<int>(env.type()), " for connection ", msg_evt.conn_id.netstack_id.value,
            "/", msg_evt.conn_id.conn_id.value);
#endif
        return result;
    }

    utils::metrics::counters().inbound_msgs_total.fetch_add(1, std::memory_order_relaxed);
    utils::metrics::inc_type(utils::metrics::counters().inbound_msgs_by_type,
                             static_cast<uint32_t>(env.type()));

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
