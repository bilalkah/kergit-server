#ifndef UTILS_METRICS_H
#define UTILS_METRICS_H

#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

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
    std::atomic<uint64_t> dropped_outbound_overflow_low_pri{0};
    std::atomic<uint64_t> dropped_outbound_overflow_high_pri{0};
    std::atomic<uint64_t> parse_fail_total{0};
    std::atomic<uint64_t> auth_fail_total{0};
    std::atomic<uint64_t> membership_fail_total{0};
    std::atomic<uint64_t> outbound_backpressure_total{0};
    std::atomic<uint64_t> dropped_inbound_low_overflow{0};
    std::atomic<uint64_t> dropped_inbound_high_overflow{0};
    std::atomic<uint64_t> evicted_inbound_low_for_high{0};
    std::atomic<uint64_t> payload_parse_total{0};
    std::atomic<uint64_t> payload_parse_fail_total{0};
    std::atomic<uint64_t> parsed_payload_violation_total{0};
    std::atomic<uint64_t> registry_view_access_total{0};
    std::atomic<uint64_t> registry_miss_total{0};
    // Number of outbound messages processed without copying ConnectionContext.
    // Increments once per outbound message, independent of fan-out size.
    std::atomic<uint64_t> registry_copy_eliminated_total{0};
    // Increments once per fan-out broadcast that uses a subscriber snapshot.
    std::atomic<uint64_t> fanout_subscriber_snapshot_total{0};
    std::atomic<uint64_t> fanout_payload_shared_total{0};
    std::atomic<uint64_t> per_conn_queue_enqueued_total{0};
    std::atomic<uint64_t> per_conn_queue_dropped_low_total{0};
    std::atomic<uint64_t> per_conn_queue_overflow_total{0};
    std::atomic<uint64_t> slow_connection_dropped_total{0};
    std::atomic<uint64_t> outbound_flush_total{0};
    std::atomic<uint64_t> outbound_flush_empty_total{0};
    std::atomic<uint64_t> outbound_flush_send_fail_total{0};
    std::atomic<uint64_t> outbound_update_auth_state_total{0};
    std::atomic<uint64_t> outbound_drop_connection_total{0};
    std::atomic<uint64_t> outbound_backpressured_total{0};
    std::atomic<uint64_t> db_write_enqueued_total{0};
    std::atomic<uint64_t> db_write_dropped_total{0};
    std::atomic<uint64_t> db_write_retry_total{0};
    std::atomic<uint64_t> db_write_success_total{0};
    std::atomic<uint64_t> db_write_fail_total{0};
    std::atomic<uint64_t> active_connections{0};
    std::atomic<uint64_t> active_users{0};
    std::atomic<uint64_t> http_health_rtt_ms{0};

    // Client RTT tracking (from WebSocket heartbeat)
    std::atomic<uint64_t> client_rtt_sum_ms{0};
    std::atomic<uint64_t> client_rtt_count{0};
    std::atomic<uint64_t> client_rtt_max_ms{0};

    // Per-port metrics (supports up to 16 ports)
    static constexpr std::size_t kMaxPorts = 16;
    std::array<std::atomic<uint64_t>, kMaxPorts> connections_by_port{};

    std::array<std::atomic<uint64_t>, 6> outbound_msgs_per_tick_buckets{};

    // Worker pool performance metrics
    std::atomic<uint64_t> active_workers{0};
    std::atomic<uint64_t> total_workers{0};
    std::atomic<uint64_t> current_queue_depth{0};
    std::atomic<uint64_t> commands_processed_total{0};
    std::atomic<uint64_t> db_write_queue_depth{0};
    std::atomic<uint64_t> db_write_queue_highwater{0};

    // Per-command execution time (microseconds)
    std::array<std::atomic<uint64_t>, kEnvelopeTypeSlots> cmd_exec_time_sum_us{};
    std::array<std::atomic<uint64_t>, kEnvelopeTypeSlots> cmd_exec_time_count{};
    std::array<std::atomic<uint64_t>, kEnvelopeTypeSlots> cmd_exec_time_max_us{};

    Counters() {
        for (auto& c : inbound_msgs_by_type) c.store(0, std::memory_order_relaxed);
        for (auto& c : outbound_msgs_by_type) c.store(0, std::memory_order_relaxed);
        for (auto& c : outbound_msgs_per_tick_buckets) c.store(0, std::memory_order_relaxed);
        for (auto& c : connections_by_port) c.store(0, std::memory_order_relaxed);
        for (auto& c : cmd_exec_time_sum_us) c.store(0, std::memory_order_relaxed);
        for (auto& c : cmd_exec_time_count) c.store(0, std::memory_order_relaxed);
        for (auto& c : cmd_exec_time_max_us) c.store(0, std::memory_order_relaxed);
    }
};

Counters& counters();

void maybe_log();

struct SnapshotCounters {
    uint64_t inbound_total{0};
    uint64_t outbound_total{0};
    uint64_t parse_fail{0};
    uint64_t auth_fail{0};
    uint64_t membership_fail{0};
    uint64_t payload_parse_total{0};
    uint64_t payload_parse_fail_total{0};
    uint64_t parsed_payload_violation_total{0};
    uint64_t registry_view_access_total{0};
    uint64_t registry_miss_total{0};
    uint64_t registry_copy_elim_total{0};
    uint64_t fanout_sub_snapshot_total{0};
    uint64_t fanout_payload_shared_total{0};
    uint64_t per_conn_enqueued_total{0};
    uint64_t per_conn_dropped_low_total{0};
    uint64_t per_conn_overflow_total{0};
    uint64_t slow_connection_dropped_total{0};
    uint64_t outbound_flush_total{0};
    uint64_t outbound_flush_empty_total{0};
    uint64_t outbound_flush_send_fail_total{0};
    uint64_t outbound_update_auth_state_total{0};
    uint64_t outbound_drop_connection_total{0};
    uint64_t outbound_backpressured_total{0};
    uint64_t db_write_enqueued_total{0};
    uint64_t db_write_dropped_total{0};
    uint64_t db_write_retry_total{0};
    uint64_t db_write_success_total{0};
    uint64_t db_write_fail_total{0};
    uint64_t dropped_in{0};
    uint64_t dropped_in_low{0};
    uint64_t dropped_in_high{0};
    uint64_t evicted_in_low_for_high{0};
    uint64_t dropped_out{0};
    uint64_t dropped_out_low{0};
    uint64_t dropped_out_high{0};
    uint64_t outbound_backpressure{0};
};

struct SnapshotGauges {
    uint64_t event_hiwat{0};
    uint64_t outbound_hiwat{0};
    uint64_t active_connections{0};
    uint64_t active_users{0};
    uint64_t http_health_rtt_ms{0};
    uint64_t client_rtt_avg_ms{0};
    uint64_t client_rtt_max_ms{0};
    std::array<uint64_t, Counters::kMaxPorts> connections_by_port{};
    uint64_t active_workers{0};
    uint64_t total_workers{0};
    uint64_t current_queue_depth{0};
    uint64_t worker_utilization_pct{0};
    uint64_t db_write_queue_depth{0};
    uint64_t db_write_queue_hiwat{0};
};

struct SnapshotHistograms {
    std::array<uint64_t, 6> outbound_tick_hist{};
};

struct CommandTimings {
    uint32_t type{0};
    std::string name;
    uint64_t avg_us{0};
    uint64_t max_us{0};
    uint64_t count{0};
};

struct MetricsSnapshot {
    uint64_t timestamp_sec{0};
    SnapshotCounters counters{};
    SnapshotGauges gauges{};
    SnapshotHistograms histograms{};
    std::vector<CommandTimings> command_timings{};
};

MetricsSnapshot snapshot_now();
std::vector<MetricsSnapshot> timeseries(uint32_t window_sec);
void start_timeseries();
void stop_timeseries();

inline void update_highwater(std::atomic<uint64_t>& highwater, uint64_t value) {
    uint64_t current = highwater.load(std::memory_order_relaxed);
    while (value > current &&
           !highwater.compare_exchange_weak(current, value, std::memory_order_relaxed)) {
    }
}

inline void inc_type(std::array<std::atomic<uint64_t>, kEnvelopeTypeSlots>& arr, uint32_t type) {
    if (type < kEnvelopeTypeSlots) {
        arr[type].fetch_add(1, std::memory_order_relaxed);
    }
}

inline void observe_outbound_msgs_per_tick(uint64_t count) {
    std::size_t bucket = 0;
    if (count == 0) {
        bucket = 0;
    } else if (count <= 10) {
        bucket = 1;
    } else if (count <= 50) {
        bucket = 2;
    } else if (count <= 200) {
        bucket = 3;
    } else if (count <= 1000) {
        bucket = 4;
    } else {
        bucket = 5;
    }
    counters().outbound_msgs_per_tick_buckets[bucket].fetch_add(1, std::memory_order_relaxed);
}

inline void observe_client_rtt(uint64_t rtt_ms) {
    auto& c = counters();
    c.client_rtt_sum_ms.fetch_add(rtt_ms, std::memory_order_relaxed);
    c.client_rtt_count.fetch_add(1, std::memory_order_relaxed);

    // Update max RTT
    uint64_t current_max = c.client_rtt_max_ms.load(std::memory_order_relaxed);
    while (rtt_ms > current_max && !c.client_rtt_max_ms.compare_exchange_weak(
                                       current_max, rtt_ms, std::memory_order_relaxed)) {
    }
}

inline void inc_port_connections(std::size_t port_index) {
    if (port_index < Counters::kMaxPorts) {
        counters().connections_by_port[port_index].fetch_add(1, std::memory_order_relaxed);
    }
}

inline void dec_port_connections(std::size_t port_index) {
    if (port_index < Counters::kMaxPorts) {
        auto& counter = counters().connections_by_port[port_index];
        uint64_t current = counter.load(std::memory_order_relaxed);
        while (current > 0 &&
               !counter.compare_exchange_weak(current, current - 1, std::memory_order_relaxed)) {
        }
    }
}

inline void observe_command_exec_time(uint32_t type, uint64_t duration_us) {
    if (type >= kEnvelopeTypeSlots) return;
    auto& c = counters();
    c.cmd_exec_time_sum_us[type].fetch_add(duration_us, std::memory_order_relaxed);
    c.cmd_exec_time_count[type].fetch_add(1, std::memory_order_relaxed);
    c.commands_processed_total.fetch_add(1, std::memory_order_relaxed);

    // Update max exec time
    uint64_t current_max = c.cmd_exec_time_max_us[type].load(std::memory_order_relaxed);
    while (duration_us > current_max && !c.cmd_exec_time_max_us[type].compare_exchange_weak(
                                            current_max, duration_us, std::memory_order_relaxed)) {
    }
}

inline void inc_active_workers() {
    counters().active_workers.fetch_add(1, std::memory_order_relaxed);
}

inline void dec_active_workers() {
    auto& counter = counters().active_workers;
    uint64_t current = counter.load(std::memory_order_relaxed);
    while (current > 0 &&
           !counter.compare_exchange_weak(current, current - 1, std::memory_order_relaxed)) {
    }
}

inline void set_total_workers(uint64_t count) {
    counters().total_workers.store(count, std::memory_order_relaxed);
}

inline void set_queue_depth(uint64_t depth) {
    counters().current_queue_depth.store(depth, std::memory_order_relaxed);
}

inline void set_db_write_queue_depth(uint64_t depth) {
    auto& c = counters();
    c.db_write_queue_depth.store(depth, std::memory_order_relaxed);
    update_highwater(c.db_write_queue_highwater, depth);
}

}  // namespace utils::metrics

#endif  // UTILS_METRICS_H
