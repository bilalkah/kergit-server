#ifndef APP_COMMANDREGISTERY_H
#define APP_COMMANDREGISTERY_H

#include "app/Dispatcher.h"

class PersistenceGateway;

namespace net {
class ClientGateway;
class ConnectionManager;
}  // namespace net

namespace app::services {
class HubPublisher;
class PublicIdService;
}  // namespace app::services

namespace app {
void register_all(Dispatcher& d, PersistenceGateway& db, net::ClientGateway& gateway,
                  net::ConnectionManager& connections, app::services::HubPublisher& hub_pub,
                  app::services::PublicIdService& ids);
}  // namespace app

#endif  // APP_COMMANDREGISTERY_H
