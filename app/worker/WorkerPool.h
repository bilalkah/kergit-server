#ifndef APP_WORKER_WORKERPOOL_H
#define APP_WORKER_WORKERPOOL_H

#include "app/Dispatcher.h"
#include "app/queue/EventQueue.h"
#include "app/queue/OutgoingQueue.h"
#include "infra/security/validation/MessageValidator.h"
#include "utils/Loggable.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <vector>
#include <unordered_map>

namespace app {

struct WorkerPoolConfig {
    std::size_t worker_count{2};
};

class WorkerPool : public utils::Loggable {
   public:
    WorkerPool(EventQueue& in_queue, OutgoingQueue& out_queue, Dispatcher& dispatcher,
               WorkerPoolConfig config = {});

    ~WorkerPool();

    void start();
    void stop();

    void pause();   // stop consuming new commands
    void resume();  // continue consuming

    bool is_paused() const { return paused_.load(); }
    bool is_running() const { return running_.load(); }

   private:
    EventQueue& in_queue_;
    OutgoingQueue& out_queue_;
    Dispatcher& dispatcher_;
    infra::security::validation::MessageValidator message_validator_{dispatcher_};

    WorkerPoolConfig config_;
    std::atomic_bool running_{false};
    std::atomic_bool paused_{false};
    std::vector<std::thread> workers_;
    // cache for currently executing commands to prevent duplicate processing
    std::unordered_map<UserId, std::string, UserIdHash, UserIdEq> executing_commands_;
    std::mutex executing_commands_mtx_;

    // pause gate
    mutable std::mutex pause_mtx_;
    std::condition_variable pause_cv_;

    void worker_loop(std::size_t worker_index);
    void wait_if_paused();

    void send_error(const CommandRequest& req, std::string_view code, std::string_view message);
};

}  // namespace app

#endif  // APP_WORKER_WORKERPOOL_H
