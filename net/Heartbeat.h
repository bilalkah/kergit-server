#ifndef NET_HEARTBEAT_H
#define NET_HEARTBEAT_H

#include "core/IApp.h"
#include "core/Types.h"
#include "net/ConnectionManager.h"
#include "net/PerSocketData.h"

#include <atomic>
#include <chrono>
#include <nlohmann/json.hpp>

struct us_timer_t;

namespace net {

struct HeartbeatConfig {
    std::chrono::milliseconds interval{2500};
    std::chrono::milliseconds timeout{8000};
    int close_code = 4000;
    const char* close_reason = "pong timeout";
};

class Heartbeat {
   public:
    Heartbeat(core::IApp& app, ConnectionManager& conns, HeartbeatConfig cfg = {});
    ~Heartbeat();

    void start();
    void stop();

    void on_open(PerSocketData& psd);
    void on_pong(PerSocketData& psd);

    std::string conn_status_message() const { return conn_status_msg_.dump(); }

   private:
    static void on_timer(us_timer_t* timer);
    void tick();

    core::IApp& app_;
    ConnectionManager& conns_;
    HeartbeatConfig cfg_;
    std::atomic<bool> running_{false};
    ::us_timer_t* timer_{nullptr};

    nlohmann::json conn_status_msg_{{"type", "conn_status"}, {"status", "alive"}, {"rtt_ms", "0"}};
};

}  // namespace net

#endif  // NET_HEARTBEAT_H
