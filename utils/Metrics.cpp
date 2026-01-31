#include "utils/Metrics.h"

#include "utils/Logger.h"

#include <chrono>
#include <fmt/format.h>

namespace utils::metrics {
namespace {
constexpr auto kLogInterval = std::chrono::seconds(5);
std::atomic<uint64_t> last_log_ns{0};
}  // namespace

Counters& counters() {
    static Counters instance;
    return instance;
}

void maybe_log() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    const auto now_ns =
        static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
    auto last = last_log_ns.load(std::memory_order_relaxed);
    if (now_ns - last < static_cast<uint64_t>(
                            std::chrono::duration_cast<std::chrono::nanoseconds>(kLogInterval)
                                .count())) {
        return;
    }
    if (!last_log_ns.compare_exchange_strong(last, now_ns, std::memory_order_relaxed)) {
        return;
    }

    auto& c = counters();
    const auto inbound = c.inbound_msgs_total.load(std::memory_order_relaxed);
    const auto outbound = c.outbound_msgs_total.load(std::memory_order_relaxed);
    const auto parse_fail = c.parse_fail_total.load(std::memory_order_relaxed);
    const auto auth_fail = c.auth_fail_total.load(std::memory_order_relaxed);
    const auto membership_fail = c.membership_fail_total.load(std::memory_order_relaxed);
    const auto dropped_in = c.dropped_inbound_total.load(std::memory_order_relaxed);
    const auto dropped_out = c.dropped_outbound_total.load(std::memory_order_relaxed);
    const auto dropped_in_low =
        c.dropped_inbound_low_overflow.load(std::memory_order_relaxed);
    const auto dropped_in_high =
        c.dropped_inbound_high_overflow.load(std::memory_order_relaxed);
    const auto evicted_in_low_for_high =
        c.evicted_inbound_low_for_high.load(std::memory_order_relaxed);
    const auto outbound_backpressure =
        c.outbound_backpressure_total.load(std::memory_order_relaxed);
    const auto event_hiwat =
        c.event_queue_highwater.exchange(0, std::memory_order_relaxed);
    const auto outbound_hiwat =
        c.outbound_queue_highwater.exchange(0, std::memory_order_relaxed);
    const auto dropped_out_low =
        c.dropped_outbound_overflow_low_pri.load(std::memory_order_relaxed);
    const auto dropped_out_high =
        c.dropped_outbound_overflow_high_pri.load(std::memory_order_relaxed);
    const auto b0 = c.outbound_msgs_per_tick_buckets[0].load(std::memory_order_relaxed);
    const auto b1 = c.outbound_msgs_per_tick_buckets[1].load(std::memory_order_relaxed);
    const auto b2 = c.outbound_msgs_per_tick_buckets[2].load(std::memory_order_relaxed);
    const auto b3 = c.outbound_msgs_per_tick_buckets[3].load(std::memory_order_relaxed);
    const auto b4 = c.outbound_msgs_per_tick_buckets[4].load(std::memory_order_relaxed);
    const auto b5 = c.outbound_msgs_per_tick_buckets[5].load(std::memory_order_relaxed);

    utils::log_line(
        utils::LogLevel::INFO,
        fmt::format(
            "metrics inbound_total={} outbound_total={} parse_fail={} auth_fail={} "
            "membership_fail={} dropped_in={} dropped_in_low={} dropped_in_high={} "
            "evicted_in_low_for_high={} dropped_out={} dropped_out_low={} "
            "dropped_out_high={} outbound_backpressure={} event_hiwat={} "
            "outbound_hiwat={} outbound_tick_hist=[{} {} {} {} {} {}]",
            inbound, outbound, parse_fail, auth_fail, membership_fail, dropped_in, dropped_in_low,
            dropped_in_high, evicted_in_low_for_high, dropped_out, dropped_out_low,
            dropped_out_high, outbound_backpressure, event_hiwat, outbound_hiwat, b0, b1, b2, b3,
            b4, b5));
}

}  // namespace utils::metrics
