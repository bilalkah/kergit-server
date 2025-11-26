#ifndef APP_QUEUE_THREADSAFE_CMD_QUEUE_H
#define APP_QUEUE_THREADSAFE_CMD_QUEUE_H

#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>

template <typename T>
class ThreadSafeQueue {
   public:
    void push(T value) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _queue.push(std::move(value));
        }
        _cv.notify_one();
    }

    // Blocking pop: returns false if queue was stopped and is empty
    bool pop(T& out) {
        std::unique_lock<std::mutex> lock(_mutex);
        _cv.wait(lock, [this] { return !_queue.empty() || _stopped; });

        if (_stopped && _queue.empty()) {
            return false;  // graceful shutdown
        }

        out = std::move(_queue.front());
        _queue.pop();
        return true;
    }

    // Non-blocking pop
    std::optional<T> try_pop() {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_queue.empty()) return std::nullopt;

        T value = std::move(_queue.front());
        _queue.pop();
        return value;
    }

    // Stop queue, unblock pop()
    void stop() {
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
