#ifndef UTILS_METRICS_H
#define UTILS_METRICS_H

#include <array>
#include <atomic>
#include <cstdint>

namespace utils::metrics {

constexpr std::size_t kEnvelopeTypeSlots = 128;

struct Counters {
    std::atomic<uint64_t> inbound_msgs_total{0};
    std::array<std::atomic<uint64_t>, kEnvelopeTypeSlots> inbound_msgs_by_type{};
    std::atomic<uint64_t> outbound_msgs_total{0};
    std::array<std::atomic<uint64_t>, kEnvelopeTypeSlots> outbound_msgs_by_type{};
    std::atomic<uint64_t> event_queue_highwater{0};
    std::atomic<uint64_t> outbound_queue_highwater{0};
    std::atomic<uint64_t> dropped_inbound_total{0};
    std::atomic<uint64_t> dropped_outbound_total{0};
    std::atomic<uint64_t> parse_fail_total{0};
    std::atomic<uint64_t> auth_fail_total{0};
    std::atomic<uint64_t> membership_fail_total{0};
    std::atomic<uint64_t> outbound_backpressure_total{0};

    Counters() {
        for (auto& c : inbound_msgs_by_type) c.store(0, std::memory_order_relaxed);
        for (auto& c : outbound_msgs_by_type) c.store(0, std::memory_order_relaxed);
    }
};

Counters& counters();

void maybe_log();

inline void update_highwater(std::atomic<uint64_t>& highwater, uint64_t value) {
    uint64_t current = highwater.load(std::memory_order_relaxed);
    while (value > current &&
           !highwater.compare_exchange_weak(current, value, std::memory_order_relaxed)) {
    }
}

inline void inc_type(std::array<std::atomic<uint64_t>, kEnvelopeTypeSlots>& arr,
                     uint32_t type) {
    if (type < kEnvelopeTypeSlots) {
        arr[type].fetch_add(1, std::memory_order_relaxed);
    }
}

}  // namespace utils::metrics

#endif  // UTILS_METRICS_H
