#ifndef APP_QUEUE_THREADSAFE_CMD_QUEUE_H
#define APP_QUEUE_THREADSAFE_CMD_QUEUE_H

#include <condition_variable>
#include <expected>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>
#include <string_view>

template <typename T>
class ThreadSafeQueue {
   public:

    void push(const T& v) noexcept {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _queue.push(v);
        }
        _cv.notify_one();
    }
    void push(T&& v) noexcept {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _queue.push(std::move(v));
        }
        _cv.notify_one();
    }

    // Blocking pop: returns false if queue was stopped and is empty
    [[nodiscard]] std::expected<T, std::string_view> pop() noexcept {
        std::unique_lock<std::mutex> lock(_mutex);
        _cv.wait(lock, [this] { return !_queue.empty() || _stopped; });

        if (_stopped && _queue.empty()) {
            return std::unexpected<std::string_view>("Queue is stopped and empty");
        }

        auto out = std::move(_queue.front());
        _queue.pop();
        return out;
    }

    // Non-blocking pop
    [[nodiscard]] std::expected<T, std::string_view> try_pop() noexcept {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_queue.empty()) return std::unexpected<std::string_view>("Queue is empty");

        T value = std::move(_queue.front());
        _queue.pop();
        return value;
    }

    // Stop queue, unblock pop()
    void stop() noexcept {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _stopped = true;
        }
        _cv.notify_all();
    }

   private:
    std::queue<T> _queue;
    std::mutex _mutex;
    std::condition_variable _cv;
    bool _stopped{false};
};

#endif  // APP_QUEUE_THREADSAFE_CMD_QUEUE_H
