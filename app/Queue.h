#ifndef APP_QUEUE_H
#define APP_QUEUE_H

#include "app/ThreadSafeCmdQueue.h"

using CommandQueue  = ThreadSafeQueue<CommandRequest>;
using OutgoingQueue = ThreadSafeQueue<OutgoingMessage>;

inline CommandQueue  g_in_queue;
inline OutgoingQueue g_out_queue;

#endif  // APP_QUEUE_H
