
#ifndef NET_TRANSPORT_WEBSOCKET_IWSAPP_H
#define NET_TRANSPORT_WEBSOCKET_IWSAPP_H

#include "net/transport/ILoop.h"
#include "net/transport/websocket/UwsTypes.h"

#include <functional>
#include <string>

namespace net::transport::websocket {

class IApp : public ILoop {
   public:
    virtual ~IApp() = default;

    // Access the underlying uWS app to register routes
    virtual UwsApp& uws() = 0;
};

}  // namespace net::transport::websocket

#endif  // NET_TRANSPORT_WEBSOCKET_IWSAPP_H
