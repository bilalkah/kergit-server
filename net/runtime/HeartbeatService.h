#ifndef NET_RUNTIME_HEARTBEATSERVICE_H
#define NET_RUNTIME_HEARTBEATSERVICE_H

#include "net/connection/ConnectionRegistery.h"
#include "net/transport/ILoop.h"

#include <atomic>
#include <chrono>
#include <fmt/format.h>

struct us_timer_t;

namespace net::runtime {

struct HeartbeatConfig {
    std::chrono::milliseconds interval{2500};
    std::chrono::milliseconds timeout{8000};
    int close_code = 4000;
    const char* close_reason = "pong timeout";
};

class HeartbeatService {
   public:
    HeartbeatService(transport::ILoop& loop, connection::ConnectionRegistery& conns,
                     HeartbeatConfig cfg = {});
    ~HeartbeatService();

    void start();
    void stop();

    void on_open(ConnId conn_id);
    std::expected<std::string, connection::ConnectionError> on_pong(ConnId conn_id);

   private:
    static void on_timer(us_timer_t* timer);

    void tick();

    /**
     * References
     */
    transport::ILoop& loop_;
    connection::ConnectionRegistery& conns_;

    /**
     * Configuration
     */
    HeartbeatConfig cfg_;

    /**
     * Runtime state
     */
    std::atomic<bool> running_{false};
    ::us_timer_t* timer_{nullptr};

    /**
     * Helper to make connection status message
     */
    inline std::string make_conn_status_msg(bool alive, int rtt_ms) const {
        const char* status = alive ? "alive" : "stale";
        return fmt::format(R"({{"type":"conn_status","status":"{}","rtt_ms":{}}})", status,
                           rtt_ms);
    };
};

}  // namespace net::runtime

#endif  // NET_RUNTIME_HEARTBEATSERVICE_H
