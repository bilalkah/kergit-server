#ifndef APP_SERVICES_MESSAGE_ASYNC_MESSAGE_WRITER_H
#define APP_SERVICES_MESSAGE_ASYNC_MESSAGE_WRITER_H

#include "app/services/message/DbWriteQueue.h"
#include "domains/Message.h"
#include "infra/persistence/repositories/MessageRepository.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <thread>

namespace app::services {

class AsyncMessageWriter {
   public:
    AsyncMessageWriter(MessageRepository& repo, std::size_t capacity, std::size_t max_retries,
                       std::chrono::milliseconds retry_delay);

    void start();
    void stop();

    [[nodiscard]] bool enqueue(Message msg);
    [[nodiscard]] std::size_t queue_depth() const;

   private:
    struct WriteRequest {
        Message msg;
        std::size_t attempt{0};
    };

    void worker_loop(std::stop_token st);
    std::chrono::milliseconds backoff_delay(std::size_t attempt) const;

    MessageRepository& repo_;
    DbWriteQueue<WriteRequest> queue_;
    std::size_t max_retries_{0};
    std::chrono::milliseconds retry_delay_{0};
    std::atomic_bool running_{false};
    std::jthread worker_;
};

}  // namespace app::services

#endif  // APP_SERVICES_MESSAGE_ASYNC_MESSAGE_WRITER_H
