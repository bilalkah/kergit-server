#ifndef APP_TSQUEUE_H
#define APP_TSQUEUE_H

#include "domains/ids/Ids.h"

#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>
#include <string>

struct CommandRequest {
    std::string command_name; 
    std::string payload; 

    ConnId conn_id;
    UserId user_id;
    HubId current_hub_id;
    ChannelId current_channel_id;
    bool authenticated{false};
};

struct OutgoingMessage {
    ConnId conn_id;

    std::string payload;

    std::function<void(struct PerSocketData&)> apply_psd;
};

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

    // Blocking pop
    T pop() {
        std::unique_lock<std::mutex> lock(_mutex);
        _cv.wait(lock, [this] { return !_queue.empty() || _stopped; });

        if (_stopped && _queue.empty()) {
            throw std::runtime_error("Queue stopped");
        }

        T value = std::move(_queue.front());
        _queue.pop();
        return value;
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

#endif  // APP_TSQUEUE_H
