#ifndef NET_OUTBOUND_OUTGOINGQUEUE_H
#define NET_OUTBOUND_OUTGOINGQUEUE_H

#include "net/outbound/IOutBoundSink.h"
#include "net/outbound/Msg.h"
#include "utils/Metrics.h"

#include <deque>
#include <mutex>
#include <type_traits>
#include <utility>

namespace net::outbound {

class OutgoingQueue : public IOutboundSink {
   public:
    explicit OutgoingQueue(std::size_t capacity = 50000) : capacity_(capacity) {}
    ~OutgoingQueue() override = default;

    PushResult push(const OutgoingMessage& msg) override { return push_impl(msg); }

    PushResult push(OutgoingMessage&& msg) override { return push_impl(std::move(msg)); }

    static_assert(std::is_move_constructible_v<OutgoingMessage>,
                  "OutgoingMessage must be move-constructible");

    [[nodiscard]] bool pop(OutgoingMessage& out) noexcept { return try_pop(out); }

    [[nodiscard]] bool try_pop(OutgoingMessage& out) noexcept {
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

   private:
    template <typename Msg>
    PushResult push_impl(Msg&& msg) {
        std::lock_guard<std::mutex> lock(mu_);
        if (capacity_ > 0 && size_ >= capacity_) {
            if (msg.priority == OutboundPriority::Low) {
                record_drop_low();
                return PushResult::DroppedLowPriority;
            }
            if (!low_.empty()) {
                low_.pop_back();
                --size_;
                record_drop_low();
            } else {
                record_drop_high();
                return PushResult::DroppedHighPriority;
            }
        }

        if (msg.priority == OutboundPriority::Low) {
            low_.push_back(std::forward<Msg>(msg));
        } else {
            high_.push_back(std::forward<Msg>(msg));
        }
        ++size_;
        utils::metrics::update_highwater(utils::metrics::counters().outbound_queue_highwater,
                                         size_);
        return PushResult::Ok;
    }

    void record_drop_low() {
        auto& c = utils::metrics::counters();
        c.dropped_outbound_total.fetch_add(1, std::memory_order_relaxed);
        c.dropped_outbound_overflow_low_pri.fetch_add(1, std::memory_order_relaxed);
    }

    void record_drop_high() {
        auto& c = utils::metrics::counters();
        c.dropped_outbound_total.fetch_add(1, std::memory_order_relaxed);
        c.dropped_outbound_overflow_high_pri.fetch_add(1, std::memory_order_relaxed);
    }

    mutable std::mutex mu_;
    std::deque<OutgoingMessage> high_;
    std::deque<OutgoingMessage> low_;
    std::size_t capacity_{0};
    std::size_t size_{0};
};

}  // namespace net::outbound
#endif  // NET_OUTBOUND_OUTGOINGQUEUE_H
