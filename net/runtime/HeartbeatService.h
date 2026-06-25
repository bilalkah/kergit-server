#ifndef NET_RUNTIME_HEARTBEATSERVICE_H
#define NET_RUNTIME_HEARTBEATSERVICE_H

#include "net/connection/ConnectionRegistery.h"
#include "net/transport/ILoop.h"

#include <atomic>
#include <chrono>

struct us_timer_t;

namespace net::runtime {

struct HeartbeatConfig {
    // How often the sweep runs.
    std::chrono::seconds interval{5};
    // Close a connection that has not sent an app-level PING for this long. Generous on
    // purpose: a backgrounded tab throttles its JS timers (Chrome ~1/min), so the timeout
    // must tolerate slow background pings and only reap genuinely gone/frozen clients.
    std::chrono::seconds timeout{75};
    std::chrono::seconds auth_pending_timeout{5};
    int close_code = 4000;
    const char* close_reason = "client_heartbeat_timeout";
};

class HeartbeatService {
   public:
    HeartbeatService(transport::ILoop& loop, connection::ConnectionRegistery& conns,
                     HeartbeatConfig cfg = {});
    ~HeartbeatService();

    void start();
    void stop();

    void on_open(ConnId conn_id);

    // Mark a connection as alive. Called on the event-loop thread from the transport
    // whenever an app-level PING arrives, so the liveness sweep keeps it.
    void note_seen(const ConnId& conn_id);

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
};

}  // namespace net::runtime

#endif  // NET_RUNTIME_HEARTBEATSERVICE_H
