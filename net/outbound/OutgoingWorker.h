#ifndef NET_OUTBOUND_OUTGOINGWORKER_H
#define NET_OUTBOUND_OUTGOINGWORKER_H

#include "net/connection/ConnectionRegistery.h"
#include "net/outbound/OutgoingQueue.h"
#include "net/transport/ILoop.h"
#include "utils/Loggable.h"

#include <atomic>
#include <chrono>

struct us_timer_t;

namespace net::outbound {

struct OutgoingWorkerConfig {
    std::chrono::milliseconds interval{5};
};

class OutgoingWorker : public utils::Loggable {
   public:
    OutgoingWorker(transport::ILoop& loop, connection::ConnectionRegistery& conns,
                   OutgoingQueue& out_q, OutgoingWorkerConfig cfg = {});
    ~OutgoingWorker();

    void start();
    void stop();

   private:
    static void on_timer(us_timer_t* timer);
    void tick();

    /**
     * References
     */
    transport::ILoop& loop_;
    connection::ConnectionRegistery& conns_;
    OutgoingQueue& out_q_;

    /**
     * Configuration
     */
    OutgoingWorkerConfig cfg_;

    /**
     * Runtime state
     */
    std::atomic<bool> running_{false};
    ::us_timer_t* timer_{nullptr};
};

};  // namespace net::outbound

#endif  // NET_OUTBOUND__OUTGOINGWORKER_H
