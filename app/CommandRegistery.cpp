#include "app/CommandRegistery.h"

#include "app/commands/AuthCommand.h"
#include "app/commands/PingCommand.h"
#include "app/services/HubPublisher.h"
#include "infra/persistence/chatdb.h"
#include "net/ClientGateway.h"
#include "net/ConnectionManager.h"

namespace app {
void register_all(Dispatcher& d, ChatDB& db, net::ClientGateway& gateway,
                  net::ConnectionManager& connections, services::HubPublisher& hub_pub) {
    d.register_cmd("ping", std::make_unique<PingCommand>());
    d.register_cmd("auth",
                   std::make_unique<AuthCommand>(db, gateway, connections, hub_pub));
    // later: list_hubs, channels_for_hub, join_channel, send_message, ...
}
}  // namespace app
