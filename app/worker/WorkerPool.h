#ifndef APP_WORKER_WORKERPOOL_H
#define APP_WORKER_WORKERPOOL_H

#include "app/dispatcher/Dispatcher.h"
#include "app/queue/EventQueue.h"
#include "core/ServerConfig.h"
#include "infra/security/validation/JsonValidator.h"
#include "infra/security/validation/ProtoValidator.h"
#include "net/outbound/IOutBoundSink.h"
#include "utils/Loggable.h"

// Protobuf
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"

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

    infra::security::validation::MessageValidator message_validator_;
    infra::security::validation::ProtoMessageValidator proto_validator_;

    core::AppStackConfig config_;
    std::atomic_bool running_{false};
    std::atomic_bool paused_{false};
    std::vector<std::jthread> workers_;

    /**
     * In-flight command tracking to prevent duplicates
     * try_mark_executing: marks a command as executing for a given connection.
     * unmark_executing: unmarks a command as executing for a given connection.
     */
    std::unordered_map<GlobalConnId, std::unordered_set<sercom::protocol::Envelope::Type>>
        executing_commands_;
    std::mutex executing_commands_mtx_;
    bool try_mark_executing(GlobalConnId conn, sercom::protocol::Envelope::Type type);
    void unmark_executing(GlobalConnId conn, sercom::protocol::Envelope::Type type);

    /**
     * Pause handling
     */
    mutable std::mutex pause_mtx_;
    std::condition_variable pause_cv_;
    void worker_loop(std::size_t worker_index);
    void wait_if_paused();

    /**
     * Prepare a serialized error message envelope
     */
    std::string prepare_error_msg(const GlobalConnId& req, sercom::protocol::Envelope::Type type,
                                  sercom::protocol::event::CommandErrorCode code,
                                  std::string_view message);

    /**
     * Handle different event types
     */
    std::vector<net::outbound::OutgoingMessage> handle_event(queue::MessageEvent& msg_evt);
    std::vector<net::outbound::OutgoingMessage> handle_event(const queue::ConnectionEvent& evt);
    std::vector<net::outbound::OutgoingMessage> handle_event(const queue::DisconnectionEvent& evt);
};

}  // namespace app::worker

#endif  // APP_WORKER_WORKERPOOL_H
