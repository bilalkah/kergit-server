#ifndef APP_SERVICES_CHANNEL_DB_WRITE_QUEUE_H
#define APP_SERVICES_CHANNEL_DB_WRITE_QUEUE_H

#include "core/base/ThreadSafeQueue.h"

#include <atomic>
#include <cstddef>
#include <expected>
#include <string_view>

namespace app::services {

template <typename T>
class DbWriteQueue : public ThreadSafeQueue<T> {
   public:
    explicit DbWriteQueue(std::size_t capacity) : capacity_(capacity) {}

    [[nodiscard]] bool try_push(const T& v) noexcept {
        if (!reserve_slot_()) return false;
        ThreadSafeQueue<T>::push(v);
        return true;
    }

    [[nodiscard]] bool try_push(T&& v) noexcept {
        if (!reserve_slot_()) return false;
        ThreadSafeQueue<T>::push(std::move(v));
        return true;
    }

    [[nodiscard]] std::expected<T, std::string_view> pop() noexcept {
        auto item = ThreadSafeQueue<T>::pop();
        if (item.has_value()) {
            size_.fetch_sub(1, std::memory_order_relaxed);
        }
        return item;
    }

    [[nodiscard]] std::expected<T, std::string_view> try_pop() noexcept {
        auto item = ThreadSafeQueue<T>::try_pop();
        if (item.has_value()) {
            size_.fetch_sub(1, std::memory_order_relaxed);
        }
        return item;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return size_.load(std::memory_order_relaxed);
    }

   private:
    [[nodiscard]] bool reserve_slot_() noexcept {
        if (capacity_ == 0) {
            size_.fetch_add(1, std::memory_order_relaxed);
            return true;
        }

        auto current = size_.load(std::memory_order_relaxed);
        while (true) {
            if (current >= capacity_) return false;
            if (size_.compare_exchange_weak(current, current + 1, std::memory_order_relaxed)) {
                return true;
            }
        }
    }

    std::size_t capacity_{0};
    std::atomic_size_t size_{0};
};

}  // namespace app::services

#endif  // APP_SERVICES_CHANNEL_DB_WRITE_QUEUE_H
