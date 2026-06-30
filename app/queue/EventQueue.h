#ifndef APP_QUEUE_EVENT_QUEUE_H
#define APP_QUEUE_EVENT_QUEUE_H

#include "app/queue/IEventSink.h"
#include "app/queue/Msg.h"
#include "utils/Metrics.h"

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <type_traits>
#include <utility>

namespace app::queue {

class EventQueue : public IEventSink {
   public:
    // DESIGN INVARIANT:
    // High-priority events represent authoritative state and must not be dropped.
    // Low-priority events are ephemeral UI signals (typing, activity, etc) and
    // may be dropped under load. Clients derive truth from snapshots/state.
    explicit EventQueue(std::size_t capacity = 30000) : capacity_(capacity) {}
    ~EventQueue() override = default;

    PushResult push(const Event& event) override { return push_impl(event); }

    PushResult push(Event&& event) override { return push_impl(std::move(event)); }

    static_assert(std::is_move_constructible_v<Event>, "Event must be move-constructible");

    [[nodiscard]] bool try_pop(Event& out) noexcept {
        std::lock_guard<std::mutex> lock(mu_);
        if (size_ == 0) {
            return false;
        }
        pop_front_locked(out);
        return true;
    }

    // Blocking pop: parks the caller on the condition variable until an event is
    // available or the queue is stopped. Returns false only when woken for
    // shutdown with nothing left to hand back. This is how workers wait, so an
    // idle worker sleeps instead of spinning and churning mu_.
    [[nodiscard]] bool wait_pop(Event& out) {
        std::unique_lock<std::mutex> lock(mu_);
        cv_.wait(lock, [this] { return size_ > 0 || stopped_; });
        if (size_ == 0) {
            return false;  // stopped and drained
        }
        pop_front_locked(out);
        return true;
    }

    std::size_t size() const noexcept {
        std::lock_guard<std::mutex> lock(mu_);
        return size_;
    }

    // Permanently wake every blocked waiter for shutdown. Idempotent.
    void stop() noexcept {
        {
            std::lock_guard<std::mutex> lock(mu_);
            stopped_ = true;
        }
        cv_.notify_all();
    }

   private:
    template <typename Ev>
    PushResult push_impl(Ev&& event) {
        const auto priority = classify_event(event);
        {
            std::lock_guard<std::mutex> lock(mu_);
            if (capacity_ > 0 && size_ >= capacity_) {
                if (priority == EventPriority::Low) {
                    record_drop_low_overflow();
                    return PushResult::DroppedLow;
                }
                if (!low_.empty()) {
                    low_.pop_back();
                    --size_;
                    record_evict_low_for_high();
                } else {
                    record_drop_high_overflow();
                    return PushResult::DroppedHigh;
                }
            }

            if (priority == EventPriority::Low) {
                low_.push_back(std::forward<Ev>(event));
            } else {
                high_.push_back(std::forward<Ev>(event));
            }
            ++size_;
            utils::metrics::update_highwater(utils::metrics::counters().event_queue_highwater,
                                             size_);
        }
        // Wake one parked worker. Notifying outside the lock avoids the woken
        // thread immediately contending on mu_.
        cv_.notify_one();
        return PushResult::Ok;
    }

    // Pop the highest-priority event. Caller must hold mu_ and ensure size_ > 0.
    void pop_front_locked(Event& out) {
        if (!high_.empty()) {
            out = std::move(high_.front());
            high_.pop_front();
        } else {
            out = std::move(low_.front());
            low_.pop_front();
        }
        --size_;
    }

    void record_drop_low_overflow() {
        auto& c = utils::metrics::counters();
        c.dropped_inbound_total.fetch_add(1, std::memory_order_relaxed);
        c.dropped_inbound_low_overflow.fetch_add(1, std::memory_order_relaxed);
    }

    void record_drop_high_overflow() {
        auto& c = utils::metrics::counters();
        c.dropped_inbound_total.fetch_add(1, std::memory_order_relaxed);
        c.dropped_inbound_high_overflow.fetch_add(1, std::memory_order_relaxed);
    }

    void record_evict_low_for_high() {
        auto& c = utils::metrics::counters();
        c.dropped_inbound_total.fetch_add(1, std::memory_order_relaxed);
        c.evicted_inbound_low_for_high.fetch_add(1, std::memory_order_relaxed);
    }

    mutable std::mutex mu_;
    std::condition_variable cv_;
    std::deque<Event> high_;
    std::deque<Event> low_;
    std::size_t capacity_{0};
    std::size_t size_{0};
    bool stopped_{false};
};

}  // namespace app::queue

#endif  // APP_QUEUE_EVENT_QUEUE_H
