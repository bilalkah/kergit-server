#ifndef APP_QUEUE_EVENT_QUEUE_H
#define APP_QUEUE_EVENT_QUEUE_H

#include "app/queue/IEventSink.h"
#include "app/queue/Msg.h"
#include "core/base/ThreadSafeQueue.h"

namespace app::queue {

class EventQueue : public IEventSink {
   public:
    EventQueue() = default;
    ~EventQueue() override = default;

    void push(const Event& event) noexcept { queue_.push(event); }

    void push(Event&& event) noexcept { queue_.push(std::move(event)); }

    [[nodiscard]] std::expected<Event, std::string_view> pop() noexcept { return queue_.pop(); }

    [[nodiscard]] std::expected<Event, std::string_view> try_pop() noexcept {
        return queue_.try_pop();
    }

    void stop() noexcept { queue_.stop(); }

   private:
    ThreadSafeQueue<Event> queue_;
};

}  // namespace app::queue

#endif  // APP_QUEUE_EVENT_QUEUE_H
