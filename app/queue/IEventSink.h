#ifndef APP_QUEUE_IEVENTSINK_H
#define APP_QUEUE_IEVENTSINK_H

#include "app/queue/Msg.h"

namespace app::queue {
class IEventSink {
   public:
    virtual ~IEventSink() = default;

    virtual PushResult push(const Event& event) = 0;

    virtual PushResult push(Event&& event) = 0;
};
}  // namespace app::queue

#endif  // APP_QUEUE_IEVENTSINK_H
