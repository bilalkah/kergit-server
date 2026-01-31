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

    const auto& c = counters();
    const auto inbound = c.inbound_msgs_total.load(std::memory_order_relaxed);
    const auto outbound = c.outbound_msgs_total.load(std::memory_order_relaxed);
    const auto parse_fail = c.parse_fail_total.load(std::memory_order_relaxed);
    const auto auth_fail = c.auth_fail_total.load(std::memory_order_relaxed);
    const auto membership_fail = c.membership_fail_total.load(std::memory_order_relaxed);
    const auto dropped_in = c.dropped_inbound_total.load(std::memory_order_relaxed);
    const auto dropped_out = c.dropped_outbound_total.load(std::memory_order_relaxed);
    const auto outbound_backpressure =
        c.outbound_backpressure_total.load(std::memory_order_relaxed);
    const auto event_hiwat = c.event_queue_highwater.load(std::memory_order_relaxed);
    const auto outbound_hiwat = c.outbound_queue_highwater.load(std::memory_order_relaxed);

    utils::log_line(
        utils::LogLevel::INFO,
        fmt::format(
            "metrics inbound_total={} outbound_total={} parse_fail={} auth_fail={} "
            "membership_fail={} dropped_in={} dropped_out={} outbound_backpressure={} "
            "event_hiwat={} outbound_hiwat={}",
            inbound, outbound, parse_fail, auth_fail, membership_fail, dropped_in, dropped_out,
            outbound_backpressure, event_hiwat, outbound_hiwat));
}

}  // namespace utils::metrics
