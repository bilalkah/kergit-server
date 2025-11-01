#ifndef CORE_IWEBSOCKET_H
#define CORE_IWEBSOCKET_H

#include <string_view>
#include "core/Types.h"
#include "net/PerSocketData.h"

class IWebSocket {
public:
  virtual ~IWebSocket() = default;

  virtual void send(std::string_view data, OpCode op = OpCode::TEXT) = 0;
  virtual PerSocketData* getUserData() = 0;

  // Optional helpers you’ll likely use
  virtual size_t getBufferedAmount() const = 0;
  virtual std::string remote_address() const = 0;
};

#endif  // CORE_IWEBSOCKET_H
