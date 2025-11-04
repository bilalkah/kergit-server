#ifndef APP_COMMANDREGISTERY_H
#define APP_COMMANDREGISTERY_H

#include "app/Dispatcher.h"

class ChatDB;

namespace net {
class ClientGateway;
class ConnectionManager;
}  // namespace net

namespace app::services {
class HubPublisher;
}

namespace app {
void register_all(Dispatcher& d, ChatDB& db, net::ClientGateway& gateway,
                  net::ConnectionManager& connections, app::services::HubPublisher& hub_pub);
}  // namespace app

#endif  // APP_COMMANDREGISTERY_H
