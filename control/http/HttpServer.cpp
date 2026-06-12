#include "control/http/HttpServer.h"

#include "App.h"
#include "utils/Metrics.h"

#include <chrono>
#include <cstdint>
#include <fmt/format.h>
#include <string>
#include <string_view>

namespace control::http {
namespace {
constexpr auto kTimeout = std::chrono::seconds(5);

uint32_t parse_u32(std::string_view s, uint32_t fallback) {
    if (s.empty()) return fallback;
    uint64_t val = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return fallback;
        val = val * 10 + static_cast<uint64_t>(c - '0');
        if (val > UINT32_MAX) return fallback;
    }
    return static_cast<uint32_t>(val);
}

std::string snapshot_to_json(const utils::metrics::MetricsSnapshot& s) {
    std::string out;
    out.reserve(1024);
    auto append_num = [&out](uint64_t value) {
        fmt::format_to(std::back_inserter(out), "{}", value);
    };

    out.append("{\"version\":1,\"ts\":");
    append_num(s.timestamp_sec);
    out.append(",\"counters\":{");

    out.append("\"inbound_total\":");
    append_num(s.counters.inbound_total);
    out.append(",");
    out.append("\"outbound_total\":");
    append_num(s.counters.outbound_total);
    out.append(",");
    out.append("\"parse_fail\":");
    append_num(s.counters.parse_fail);
    out.append(",");
    out.append("\"auth_fail\":");
    append_num(s.counters.auth_fail);
    out.append(",");
    out.append("\"membership_fail\":");
    append_num(s.counters.membership_fail);
    out.append(",");
    out.append("\"payload_parse_total\":");
    append_num(s.counters.payload_parse_total);
    out.append(",");
    out.append("\"payload_parse_fail_total\":");
    append_num(s.counters.payload_parse_fail_total);
    out.append(",");
    out.append("\"parsed_payload_violation_total\":");
    append_num(s.counters.parsed_payload_violation_total);
    out.append(",");
    out.append("\"registry_view_access_total\":");
    append_num(s.counters.registry_view_access_total);
    out.append(",");
    out.append("\"registry_miss_total\":");
    append_num(s.counters.registry_miss_total);
    out.append(",");
    out.append("\"registry_copy_elim_total\":");
    append_num(s.counters.registry_copy_elim_total);
    out.append(",");
    out.append("\"fanout_sub_snapshot_total\":");
    append_num(s.counters.fanout_sub_snapshot_total);
    out.append(",");
    out.append("\"fanout_payload_shared_total\":");
    append_num(s.counters.fanout_payload_shared_total);
    out.append(",");
    out.append("\"per_conn_enqueued_total\":");
    append_num(s.counters.per_conn_enqueued_total);
    out.append(",");
    out.append("\"per_conn_dropped_low_total\":");
    append_num(s.counters.per_conn_dropped_low_total);
    out.append(",");
    out.append("\"per_conn_overflow_total\":");
    append_num(s.counters.per_conn_overflow_total);
    out.append(",");
    out.append("\"slow_connection_dropped_total\":");
    append_num(s.counters.slow_connection_dropped_total);
    out.append(",");
    out.append("\"outbound_flush_total\":");
    append_num(s.counters.outbound_flush_total);
    out.append(",");
    out.append("\"outbound_flush_empty_total\":");
    append_num(s.counters.outbound_flush_empty_total);
    out.append(",");
    out.append("\"outbound_flush_send_fail_total\":");
    append_num(s.counters.outbound_flush_send_fail_total);
    out.append(",");
    out.append("\"outbound_backpressured_total\":");
    append_num(s.counters.outbound_backpressured_total);
    out.append(",");
    out.append("\"dropped_in\":");
    append_num(s.counters.dropped_in);
    out.append(",");
    out.append("\"dropped_in_low\":");
    append_num(s.counters.dropped_in_low);
    out.append(",");
    out.append("\"dropped_in_high\":");
    append_num(s.counters.dropped_in_high);
    out.append(",");
    out.append("\"evicted_in_low_for_high\":");
    append_num(s.counters.evicted_in_low_for_high);
    out.append(",");
    out.append("\"dropped_out\":");
    append_num(s.counters.dropped_out);
    out.append(",");
    out.append("\"dropped_out_low\":");
    append_num(s.counters.dropped_out_low);
    out.append(",");
    out.append("\"dropped_out_high\":");
    append_num(s.counters.dropped_out_high);
    out.append(",");
    out.append("\"outbound_backpressure\":");
    append_num(s.counters.outbound_backpressure);

    out.append("},\"gauges\":{");
    out.append("\"event_hiwat\":");
    append_num(s.gauges.event_hiwat);
    out.append(",");
    out.append("\"outbound_hiwat\":");
    append_num(s.gauges.outbound_hiwat);
    out.append(",");
    out.append("\"active_connections\":");
    append_num(s.gauges.active_connections);
    out.append(",");
    out.append("\"active_users\":");
    append_num(s.gauges.active_users);
    out.append(",");
    out.append("\"http_health_rtt_ms\":");
    append_num(s.gauges.http_health_rtt_ms);
    out.append(",");
    out.append("\"client_rtt_avg_ms\":");
    append_num(s.gauges.client_rtt_avg_ms);
    out.append(",");
    out.append("\"client_rtt_max_ms\":");
    append_num(s.gauges.client_rtt_max_ms);
    out.append(",");
    out.append("\"connections_by_port\":[");
    for (std::size_t i = 0; i < s.gauges.connections_by_port.size(); ++i) {
        if (i > 0) out.append(",");
        append_num(s.gauges.connections_by_port[i]);
    }
    out.append("],");
    out.append("\"active_workers\":");
    append_num(s.gauges.active_workers);
    out.append(",");
    out.append("\"total_workers\":");
    append_num(s.gauges.total_workers);
    out.append(",");
    out.append("\"current_queue_depth\":");
    append_num(s.gauges.current_queue_depth);
    out.append(",");
    out.append("\"worker_utilization_pct\":");
    append_num(s.gauges.worker_utilization_pct);

    out.append("},\"histograms\":{");
    out.append("\"outbound_tick_hist\":[");
    append_num(s.histograms.outbound_tick_hist[0]);
    out.append(",");
    append_num(s.histograms.outbound_tick_hist[1]);
    out.append(",");
    append_num(s.histograms.outbound_tick_hist[2]);
    out.append(",");
    append_num(s.histograms.outbound_tick_hist[3]);
    out.append(",");
    append_num(s.histograms.outbound_tick_hist[4]);
    out.append(",");
    append_num(s.histograms.outbound_tick_hist[5]);
    out.append("]},\"command_timings\":[");
    for (std::size_t i = 0; i < s.command_timings.size(); ++i) {
        if (i > 0) out.append(",");
        const auto& ct = s.command_timings[i];
        out.append("{");
        out.append("\"type\":");
        append_num(ct.type);
        out.append(",");
        out.append("\"name\":\"");
        out.append(ct.name);
        out.append("\",");
        out.append("\"avg_us\":");
        append_num(ct.avg_us);
        out.append(",");
        out.append("\"max_us\":");
        append_num(ct.max_us);
        out.append(",");
        out.append("\"count\":");
        append_num(ct.count);
        out.append("}");
    }
    out.append("]}");

    return out;
}
}  // namespace

HttpServer::HttpServer(core::ControlPlaneConfig cfg) : cfg_(std::move(cfg)) {}

HttpServer::~HttpServer() { stop(); }

bool HttpServer::start() {
    if (starting_.exchange(true)) return true;
    started_.store(false, std::memory_order_release);
    thread_ = std::jthread(&HttpServer::run, this);
    auto start_time = std::chrono::system_clock::now();
    while (!started_.load()) {
        if (std::chrono::system_clock::now() - start_time > kTimeout) {
            starting_.store(false, std::memory_order_release);
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return true;
}

void HttpServer::stop() {
    if (stopped_.exchange(true)) return;
    if (stop_requested_.exchange(true)) return;
    if (auto* loop = static_cast<uWS::Loop*>(loop_.load(std::memory_order_acquire))) {
        auto* app = static_cast<uWS::App*>(app_);
        loop->defer([app]() {
            if (app) app->close();
        });
    }
    if (thread_.joinable()) {
        thread_.join();
    }
}

void HttpServer::run() {
    uWS::App app;
    app_ = &app;
    loop_.store(uWS::Loop::get(), std::memory_order_release);
    auto write_cors = [](auto* res) {
        res->writeHeader("Access-Control-Allow-Origin", "*");
        res->writeHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
        res->writeHeader("Access-Control-Allow-Headers", "Content-Type");
    };
    app.options("/*", [write_cors](auto* res, auto*) {
        write_cors(res);
        res->end();
    });
    app.get("/health", [](auto* res, auto*) {
        const auto start = std::chrono::steady_clock::now();
        res->writeHeader("Access-Control-Allow-Origin", "*");
        res->writeHeader("Content-Type", "application/json");
        const auto elapsed = std::chrono::steady_clock::now() - start;
        const auto ms = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
        utils::metrics::counters().http_health_rtt_ms.store(ms, std::memory_order_relaxed);
        res->end("{\"status\":\"ok\"}");
    });
    app.get("/metrics/snapshot", [](auto* res, auto*) {
        const auto start = std::chrono::steady_clock::now();
        res->writeHeader("Access-Control-Allow-Origin", "*");
        res->writeHeader("Content-Type", "application/json");
        const auto snap = utils::metrics::snapshot_now();
        const auto elapsed = std::chrono::steady_clock::now() - start;
        const auto ms = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
        utils::metrics::counters().http_health_rtt_ms.store(ms, std::memory_order_relaxed);
        res->end(snapshot_to_json(snap));
    });
    app.get("/metrics/timeseries", [](auto* res, auto* req) {
        const auto start = std::chrono::steady_clock::now();
        res->writeHeader("Access-Control-Allow-Origin", "*");
        res->writeHeader("Content-Type", "application/json");
        const auto window = parse_u32(req->getQuery("window"), 60);
        const auto series = utils::metrics::timeseries(window);
        std::string out;
        out.reserve(series.size() * 256);
        out.append(fmt::format("{{\"version\":1,\"window_sec\":{},\"samples\":[", window));
        for (std::size_t i = 0; i < series.size(); ++i) {
            if (i) out.append(",");
            out.append(snapshot_to_json(series[i]));
        }
        out.append("]}");
        const auto elapsed = std::chrono::steady_clock::now() - start;
        const auto ms = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
        utils::metrics::counters().http_health_rtt_ms.store(ms, std::memory_order_relaxed);
        res->end(out);
    });
    app.get("/metrics/public", [](auto* res, auto*) {
        res->writeHeader("Access-Control-Allow-Origin", "*");
        res->writeHeader("Content-Type", "application/json");
        res->writeHeader("Cache-Control", "no-cache");

        const auto snap = utils::metrics::snapshot_now();
        std::string out;
        out.reserve(256);
        auto append_num = [&out](uint64_t value) {
            fmt::format_to(std::back_inserter(out), "{}", value);
        };

        out.append("{\"status\":\"ok\",");
        out.append("\"connections\":");
        append_num(snap.gauges.active_connections);
        out.append(",");
        out.append("\"users\":");
        append_num(snap.gauges.active_users);
        out.append(",");
        out.append("\"ping_ms\":");
        append_num(snap.gauges.client_rtt_avg_ms);
        out.append(",");
        out.append("\"ping_max_ms\":");
        append_num(snap.gauges.client_rtt_max_ms);
        out.append(",");
        out.append("\"active_workers\":");
        append_num(snap.gauges.active_workers);
        out.append(",");
        out.append("\"total_workers\":");
        append_num(snap.gauges.total_workers);
        out.append(",");
        out.append("\"worker_utilization_pct\":");
        append_num(snap.gauges.worker_utilization_pct);
        out.append(",");
        out.append("\"queue_depth\":");
        append_num(snap.gauges.current_queue_depth);
        out.append(",");
        out.append("\"commands_processed\":");
        append_num(
            utils::metrics::counters().commands_processed_total.load(std::memory_order_relaxed));
        out.append("}");

        res->end(out);
    });

    auto host = cfg_.host;
    auto port = cfg_.port;
    app.listen(host, port, [&](auto* token) {
        if (token) {
            started_.store(true, std::memory_order_release);
        } else {
            started_.store(false, std::memory_order_release);
        }
    });

    app.run();
    started_.store(false, std::memory_order_release);
    starting_.store(false, std::memory_order_release);
    app_ = nullptr;
}

}  // namespace control::http
