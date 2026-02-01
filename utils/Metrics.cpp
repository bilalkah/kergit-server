#include "utils/Metrics.h"

#include "utils/Logger.h"

#include <chrono>
#include <fmt/format.h>
#include <mutex>
#include <shared_mutex>
#include <thread>

namespace utils::metrics {
namespace {
constexpr auto kLogInterval = std::chrono::seconds(5);
std::atomic<uint64_t> last_log_ns{0};
constexpr std::size_t kTimeseriesCapacity = 300;
std::vector<MetricsSnapshot> ring;
std::size_t ring_index = 0;
std::size_t ring_size = 0;
std::shared_mutex ring_mu;
std::jthread timeseries_thread;
std::atomic<bool> timeseries_started{false};
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
    const auto payload_parse =
        c.payload_parse_total.load(std::memory_order_relaxed);
    const auto payload_parse_fail =
        c.payload_parse_fail_total.load(std::memory_order_relaxed);
    const auto parsed_payload_violation =
        c.parsed_payload_violation_total.load(std::memory_order_relaxed);
    const auto registry_view_access =
        c.registry_view_access_total.load(std::memory_order_relaxed);
    const auto registry_miss =
        c.registry_miss_total.load(std::memory_order_relaxed);
    const auto registry_copy_elim =
        c.registry_copy_eliminated_total.load(std::memory_order_relaxed);
    const auto fanout_sub_snap =
        c.fanout_subscriber_snapshot_total.load(std::memory_order_relaxed);
    const auto fanout_payload_shared =
        c.fanout_payload_shared_total.load(std::memory_order_relaxed);
    const auto per_conn_enq =
        c.per_conn_queue_enqueued_total.load(std::memory_order_relaxed);
    const auto per_conn_drop_low =
        c.per_conn_queue_dropped_low_total.load(std::memory_order_relaxed);
    const auto per_conn_overflow =
        c.per_conn_queue_overflow_total.load(std::memory_order_relaxed);
    const auto slow_conn_dropped =
        c.slow_connection_dropped_total.load(std::memory_order_relaxed);
    const auto outbound_flush =
        c.outbound_flush_total.load(std::memory_order_relaxed);
    const auto outbound_flush_empty =
        c.outbound_flush_empty_total.load(std::memory_order_relaxed);
    const auto outbound_flush_fail =
        c.outbound_flush_send_fail_total.load(std::memory_order_relaxed);
    const auto outbound_backpressured =
        c.outbound_backpressured_total.load(std::memory_order_relaxed);
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
            "membership_fail={} payload_parse_total={} payload_parse_fail_total={} "
            "parsed_payload_violation_total={} registry_view_access_total={} registry_miss_total={} "
            "registry_copy_elim_total={} "
            "fanout_sub_snapshot_total={} fanout_payload_shared_total={} "
            "per_conn_enqueued_total={} per_conn_dropped_low_total={} "
            "per_conn_overflow_total={} slow_connection_dropped_total={} "
            "outbound_flush_total={} outbound_flush_empty_total={} "
            "outbound_flush_send_fail_total={} outbound_backpressured_total={} dropped_in={} "
            "dropped_in_low={} dropped_in_high={} evicted_in_low_for_high={} dropped_out={} "
            "dropped_out_low={} dropped_out_high={} outbound_backpressure={} event_hiwat={} "
            "outbound_hiwat={} outbound_tick_hist=[{} {} {} {} {} {}]",
            inbound, outbound, parse_fail, auth_fail, membership_fail, payload_parse,
            payload_parse_fail, parsed_payload_violation, registry_view_access, registry_miss,
            registry_copy_elim, fanout_sub_snap, fanout_payload_shared, per_conn_enq,
            per_conn_drop_low, per_conn_overflow, slow_conn_dropped, outbound_flush,
            outbound_flush_empty, outbound_flush_fail, outbound_backpressured, dropped_in,
            dropped_in_low, dropped_in_high,
            evicted_in_low_for_high, dropped_out, dropped_out_low, dropped_out_high,
            outbound_backpressure, event_hiwat, outbound_hiwat, b0, b1, b2, b3, b4, b5));
}

static MetricsSnapshot capture_snapshot(uint64_t ts_sec) {
    auto& c = counters();
    MetricsSnapshot s{};
    s.timestamp_sec = ts_sec;
    s.inbound_total = c.inbound_msgs_total.load(std::memory_order_relaxed);
    s.outbound_total = c.outbound_msgs_total.load(std::memory_order_relaxed);
    s.parse_fail = c.parse_fail_total.load(std::memory_order_relaxed);
    s.auth_fail = c.auth_fail_total.load(std::memory_order_relaxed);
    s.membership_fail = c.membership_fail_total.load(std::memory_order_relaxed);
    s.payload_parse_total = c.payload_parse_total.load(std::memory_order_relaxed);
    s.payload_parse_fail_total = c.payload_parse_fail_total.load(std::memory_order_relaxed);
    s.parsed_payload_violation_total =
        c.parsed_payload_violation_total.load(std::memory_order_relaxed);
    s.registry_view_access_total =
        c.registry_view_access_total.load(std::memory_order_relaxed);
    s.registry_miss_total = c.registry_miss_total.load(std::memory_order_relaxed);
    s.registry_copy_elim_total =
        c.registry_copy_eliminated_total.load(std::memory_order_relaxed);
    s.fanout_sub_snapshot_total =
        c.fanout_subscriber_snapshot_total.load(std::memory_order_relaxed);
    s.fanout_payload_shared_total =
        c.fanout_payload_shared_total.load(std::memory_order_relaxed);
    s.per_conn_enqueued_total =
        c.per_conn_queue_enqueued_total.load(std::memory_order_relaxed);
    s.per_conn_dropped_low_total =
        c.per_conn_queue_dropped_low_total.load(std::memory_order_relaxed);
    s.per_conn_overflow_total =
        c.per_conn_queue_overflow_total.load(std::memory_order_relaxed);
    s.slow_connection_dropped_total =
        c.slow_connection_dropped_total.load(std::memory_order_relaxed);
    s.outbound_flush_total = c.outbound_flush_total.load(std::memory_order_relaxed);
    s.outbound_flush_empty_total =
        c.outbound_flush_empty_total.load(std::memory_order_relaxed);
    s.outbound_flush_send_fail_total =
        c.outbound_flush_send_fail_total.load(std::memory_order_relaxed);
    s.outbound_backpressured_total =
        c.outbound_backpressured_total.load(std::memory_order_relaxed);
    s.dropped_in = c.dropped_inbound_total.load(std::memory_order_relaxed);
    s.dropped_in_low = c.dropped_inbound_low_overflow.load(std::memory_order_relaxed);
    s.dropped_in_high = c.dropped_inbound_high_overflow.load(std::memory_order_relaxed);
    s.evicted_in_low_for_high =
        c.evicted_inbound_low_for_high.load(std::memory_order_relaxed);
    s.dropped_out = c.dropped_outbound_total.load(std::memory_order_relaxed);
    s.dropped_out_low = c.dropped_outbound_overflow_low_pri.load(std::memory_order_relaxed);
    s.dropped_out_high = c.dropped_outbound_overflow_high_pri.load(std::memory_order_relaxed);
    s.outbound_backpressure = c.outbound_backpressure_total.load(std::memory_order_relaxed);
    s.event_hiwat = c.event_queue_highwater.load(std::memory_order_relaxed);
    s.outbound_hiwat = c.outbound_queue_highwater.load(std::memory_order_relaxed);
    for (std::size_t i = 0; i < s.outbound_tick_hist.size(); ++i) {
        s.outbound_tick_hist[i] =
            c.outbound_msgs_per_tick_buckets[i].load(std::memory_order_relaxed);
    }
    return s;
}

MetricsSnapshot snapshot_now() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto ts_sec =
        static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(now).count());
    return capture_snapshot(ts_sec);
}

std::vector<MetricsSnapshot> timeseries(uint32_t window_sec) {
    std::shared_lock lock(ring_mu);
    if (ring_size == 0) return {};
    const std::size_t window =
        std::min<std::size_t>(window_sec, ring_size);
    std::vector<MetricsSnapshot> out;
    out.reserve(window);
    const std::size_t start =
        (ring_index + kTimeseriesCapacity - window) % kTimeseriesCapacity;
    for (std::size_t i = 0; i < window; ++i) {
        const std::size_t idx = (start + i) % kTimeseriesCapacity;
        out.push_back(ring[idx]);
    }
    return out;
}

void start_timeseries() {
    bool expected = false;
    if (!timeseries_started.compare_exchange_strong(expected, true)) return;
    ring.resize(kTimeseriesCapacity);
    timeseries_thread = std::jthread([](std::stop_token st) {
        while (!st.stop_requested()) {
            const auto now = std::chrono::system_clock::now().time_since_epoch();
            const auto ts_sec = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::seconds>(now).count());
            auto snap = capture_snapshot(ts_sec);
            {
                std::unique_lock lock(ring_mu);
                ring[ring_index] = std::move(snap);
                ring_index = (ring_index + 1) % kTimeseriesCapacity;
                ring_size = std::min<std::size_t>(ring_size + 1, kTimeseriesCapacity);
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });
}

void stop_timeseries() {
    if (!timeseries_started.exchange(false)) return;
    timeseries_thread.request_stop();
    if (timeseries_thread.joinable()) {
        timeseries_thread.join();
    }
}

}  // namespace utils::metrics
