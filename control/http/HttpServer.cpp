#include "control/http/HttpServer.h"

#include "utils/Metrics.h"

#include <chrono>
#include <cstdint>
#include <fmt/format.h>
#include <string>
#include <string_view>
#include "App.h"

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
    return fmt::format(
        "{{\"ts\":{},\"inbound_total\":{},\"outbound_total\":{},\"parse_fail\":{},"
        "\"auth_fail\":{},\"membership_fail\":{},\"payload_parse_total\":{},"
        "\"payload_parse_fail_total\":{},\"parsed_payload_violation_total\":{},"
        "\"registry_view_access_total\":{},\"registry_miss_total\":{},"
        "\"registry_copy_elim_total\":{},\"fanout_sub_snapshot_total\":{},"
        "\"fanout_payload_shared_total\":{},\"per_conn_enqueued_total\":{},"
        "\"per_conn_dropped_low_total\":{},\"per_conn_overflow_total\":{},"
        "\"slow_connection_dropped_total\":{},\"outbound_flush_total\":{},"
        "\"outbound_flush_empty_total\":{},\"outbound_flush_send_fail_total\":{},"
        "\"outbound_backpressured_total\":{},\"dropped_in\":{},\"dropped_in_low\":{},"
        "\"dropped_in_high\":{},\"evicted_in_low_for_high\":{},\"dropped_out\":{},"
        "\"dropped_out_low\":{},\"dropped_out_high\":{},\"outbound_backpressure\":{},"
        "\"event_hiwat\":{},\"outbound_hiwat\":{},\"outbound_tick_hist\":[{},{},{},{},{},{}]}}",
        s.timestamp_sec, s.inbound_total, s.outbound_total, s.parse_fail, s.auth_fail,
        s.membership_fail, s.payload_parse_total, s.payload_parse_fail_total,
        s.parsed_payload_violation_total, s.registry_view_access_total, s.registry_miss_total,
        s.registry_copy_elim_total, s.fanout_sub_snapshot_total, s.fanout_payload_shared_total,
        s.per_conn_enqueued_total, s.per_conn_dropped_low_total, s.per_conn_overflow_total,
        s.slow_connection_dropped_total, s.outbound_flush_total, s.outbound_flush_empty_total,
        s.outbound_flush_send_fail_total, s.outbound_backpressured_total, s.dropped_in,
        s.dropped_in_low, s.dropped_in_high, s.evicted_in_low_for_high, s.dropped_out,
        s.dropped_out_low, s.dropped_out_high, s.outbound_backpressure, s.event_hiwat,
        s.outbound_hiwat, s.outbound_tick_hist[0], s.outbound_tick_hist[1],
        s.outbound_tick_hist[2], s.outbound_tick_hist[3], s.outbound_tick_hist[4],
        s.outbound_tick_hist[5]);
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
    app.get("/health", [](auto* res, auto*) {
        res->writeHeader("Content-Type", "application/json");
        res->end("{\"status\":\"ok\"}");
    });
    app.get("/metrics/snapshot", [](auto* res, auto*) {
        res->writeHeader("Content-Type", "application/json");
        const auto snap = utils::metrics::snapshot_now();
        res->end(snapshot_to_json(snap));
    });
    app.get("/metrics/timeseries", [](auto* res, auto* req) {
        res->writeHeader("Content-Type", "application/json");
        const auto window =
            parse_u32(req->getQuery("window"), 60);
        const auto series = utils::metrics::timeseries(window);
        std::string out;
        out.reserve(series.size() * 64);
        out.append("[");
        for (std::size_t i = 0; i < series.size(); ++i) {
            if (i) out.append(",");
            out.append(snapshot_to_json(series[i]));
        }
        out.append("]");
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
