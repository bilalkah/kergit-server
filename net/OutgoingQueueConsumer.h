#ifndef NET_OUTGOINGQUEUECONSUMER_H
#define NET_OUTGOINGQUEUECONSUMER_H

#include "app/queue/OutgoingQueue.h"
#include "core/IApp.h"
#include "core/Types.h"
#include "net/ClientGateway.h"
#include "net/ConnectionManager.h"
#include "net/PerSocketData.h"
#include "utils/Loggable.h"

#include <atomic>
#include <chrono>
#include <nlohmann/json.hpp>

struct us_timer_t;
namespace net {

struct OutgoingConsumerConfig {
    std::chrono::milliseconds interval{5};
};

class OutgoingQueueConsumer : public utils::Loggable {
   public:
    OutgoingQueueConsumer(core::IApp& app, ConnectionManager& conns, ClientGateway& gateway,
                          OutgoingQueue& out_q, OutgoingConsumerConfig cfg = {});
    ~OutgoingQueueConsumer();

    void start();
    void stop();

   private:
    static void on_timer(us_timer_t* timer);
    void tick();

    core::IApp& app_;
    ConnectionManager& conns_;
    ClientGateway& cli_gtw_;
    OutgoingQueue& out_q_;
    OutgoingConsumerConfig cfg_;
    std::atomic<bool> running_{false};
    ::us_timer_t* timer_{nullptr};
};

};  // namespace net

#endif  // NET_OUTGOINGQUEUECONSUMER_H
