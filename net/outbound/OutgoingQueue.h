#ifndef NET_OUTBOUND_OUTGOINGQUEUE_H
#define NET_OUTBOUND_OUTGOINGQUEUE_H

#include "core/base/ThreadSafeQueue.h"
#include "net/outbound/IOutBoundSink.h"
#include "net/outbound/Msg.h"

namespace net::outbound {

class OutgoingQueue : public IOutboundSink {
   public:
    OutgoingQueue() = default;
    ~OutgoingQueue() override = default;

    void push(const OutgoingMessage& msg) noexcept { queue_.push(msg); }

    void push(OutgoingMessage&& msg) noexcept { queue_.push(std::move(msg)); }

    [[nodiscard]] std::expected<OutgoingMessage, std::string_view> pop() noexcept {
        return queue_.pop();
    }

    [[nodiscard]] std::expected<OutgoingMessage, std::string_view> try_pop() noexcept {
        return queue_.try_pop();
    }

    void stop() noexcept { queue_.stop(); }

   private:
    ThreadSafeQueue<OutgoingMessage> queue_;
};

}  // namespace net::outbound
#endif  // NET_OUTBOUND_OUTGOINGQUEUE_H
