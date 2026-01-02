#ifndef APP_WORKER_WORKERPOOL_H
#define APP_WORKER_WORKERPOOL_H

#include "app/dispatcher/Dispatcher.h"
#include "app/queue/EventQueue.h"
#include "core/ServerConfig.h"
#include "infra/security/validation/MessageValidator.h"
#include "app/worker/AuthGuard.h"
#include "net/outbound/IOutBoundSink.h"
#include "utils/Loggable.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace app::worker {

class WorkerPool : public utils::Loggable {
   public:
    WorkerPool(queue::EventQueue& in_queue, net::outbound::IOutboundSink& out_queue,
               Dispatcher& dispatcher_, CommandContext& cmd_ctx,
               core::AppStackConfig appstack_config = {});

    ~WorkerPool();

    void start();
    void stop();

    void pause();   // stop consuming new commands
    void resume();  // continue consuming

    bool is_paused() const { return paused_.load(); }
    bool is_running() const { return running_.load(); }

   private:
    queue::EventQueue& in_queue_;
    net::outbound::IOutboundSink& out_queue_;
    Dispatcher& dispatcher_;
    CommandContext& cmd_ctx_;
    AuthGuard auth_guard_;

    infra::security::validation::MessageValidator message_validator_;

    core::AppStackConfig config_;
    std::atomic_bool running_{false};
    std::atomic_bool paused_{false};
    std::vector<std::jthread> workers_;

    // cache for currently executing commands to prevent duplicate processing for connection id
    std::unordered_map<GlobalConnId, std::string> executing_commands_;
    mutable std::mutex executing_commands_mtx_;

    // pause gate
    mutable std::mutex pause_mtx_;
    std::condition_variable pause_cv_;

    void worker_loop(std::size_t worker_index);
    void wait_if_paused();

    void send_error(const GlobalConnId& req, std::string_view code, std::string_view message);
};

}  // namespace app::worker

#endif  // APP_WORKER_WORKERPOOL_H
