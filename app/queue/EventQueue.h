#ifndef APP_QUEUE_EVENT_QUEUE_H
#define APP_QUEUE_EVENT_QUEUE_H

#include "app/queue/IEventSink.h"
#include "app/queue/Msg.h"
#include "utils/Metrics.h"

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
        if (!high_.empty()) {
            out = std::move(high_.front());
            high_.pop_front();
        } else {
            out = std::move(low_.front());
            low_.pop_front();
        }
        --size_;
        return true;
    }

    std::size_t size() const noexcept {
        std::lock_guard<std::mutex> lock(mu_);
        return size_;
    }

    void stop() noexcept {}

   private:
    template <typename Ev>
    PushResult push_impl(Ev&& event) {
        const auto priority = classify_event(event);
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
        utils::metrics::update_highwater(utils::metrics::counters().event_queue_highwater, size_);
        return PushResult::Ok;
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
    std::deque<Event> high_;
    std::deque<Event> low_;
    std::size_t capacity_{0};
    std::size_t size_{0};
};

}  // namespace app::queue

#endif  // APP_QUEUE_EVENT_QUEUE_H
