#ifndef NET_WSADAPTER_H
#define NET_WSADAPTER_H

#include "core/IWebSocket.h"
#include "core/Types.h"
#include "net/PerSocketData.h"

namespace net {

template <typename SocketT>
class WSAdapter : public IWebSocket {
   public:
    explicit WSAdapter(SocketT* s) : s_(s) {}

    void send(std::string_view data, OpCode op = OpCode::TEXT) override {
        if (s_) s_->send(data, op);
    }
    PerSocketData* getUserData() override { return s_ ? s_->getUserData() : nullptr; }
    size_t getBufferedAmount() const override { return s_ ? s_->getBufferedAmount() : 0; }
    std::string remote_address() const override {
        return s_ ? std::string(s_->getRemoteAddressAsText()) : std::string{};
    }

    SocketT* raw() const { return s_; }

   private:
    SocketT* s_{nullptr};
};

using UwsSocket = UwsWebSocketT<PerSocketData>;
using UwsWSAdapter = WSAdapter<UwsSocket>;

}  // namespace net

#endif  // NET_WSADAPTER_H
