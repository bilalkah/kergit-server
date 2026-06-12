#include "app/services/message/AsyncMessageWriter.h"

#include "utils/Logger.h"
#include "utils/Metrics.h"

#include <string>

namespace app::services {

AsyncMessageWriter::AsyncMessageWriter(MessageRepository& repo, std::size_t capacity,
                                       std::size_t max_retries,
                                       std::chrono::milliseconds retry_delay)
    : repo_(repo), queue_(capacity), max_retries_(max_retries), retry_delay_(retry_delay) {}

void AsyncMessageWriter::start() {
    if (running_.exchange(true)) return;
    worker_ = std::jthread([this](std::stop_token st) { worker_loop(st); });
}

void AsyncMessageWriter::stop() {
    if (!running_.exchange(false)) return;
    queue_.stop();
    if (worker_.joinable()) {
        worker_.join();
    }
}

bool AsyncMessageWriter::enqueue(Message msg) {
    const bool ok = queue_.try_push(WriteRequest{std::move(msg), 0});
    auto& c = utils::metrics::counters();
    if (ok) {
        c.db_write_enqueued_total.fetch_add(1, std::memory_order_relaxed);
        utils::metrics::set_db_write_queue_depth(queue_.size());
    } else {
        c.db_write_dropped_total.fetch_add(1, std::memory_order_relaxed);
    }
    return ok;
}

std::size_t AsyncMessageWriter::queue_depth() const { return queue_.size(); }

std::chrono::milliseconds AsyncMessageWriter::backoff_delay(std::size_t attempt) const {
    if (retry_delay_.count() == 0) return std::chrono::milliseconds(0);
    const std::size_t capped = attempt > 6 ? 6 : attempt;
    const auto mult = static_cast<int>(1 << capped);
    return std::chrono::milliseconds(retry_delay_.count() * mult);
}

void AsyncMessageWriter::worker_loop(std::stop_token st) {
    while (!st.stop_requested()) {
        auto item = queue_.pop();
        if (!item.has_value()) return;
        auto req = std::move(item.value());

        utils::metrics::set_db_write_queue_depth(queue_.size());

        try {
            const bool ok = repo_.insertMessage(req.msg);
            if (!ok) {
                throw std::runtime_error("insertMessage returned false");
            }
            utils::metrics::counters().db_write_success_total.fetch_add(1,
                                                                        std::memory_order_relaxed);
        } catch (const std::exception& ex) {
            if (req.attempt < max_retries_) {
                utils::metrics::counters().db_write_retry_total.fetch_add(
                    1, std::memory_order_relaxed);
                std::this_thread::sleep_for(backoff_delay(req.attempt));
                ++req.attempt;
                if (!queue_.try_push(std::move(req))) {
                    utils::metrics::counters().db_write_dropped_total.fetch_add(
                        1, std::memory_order_relaxed);
                }
                continue;
            }
            utils::metrics::counters().db_write_fail_total.fetch_add(1, std::memory_order_relaxed);
            utils::log_line(utils::LogLevel::ERROR,
                            std::string("Async message write failed: ") + ex.what());
        }
    }
}

}  // namespace app::services
