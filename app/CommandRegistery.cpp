#include "app/CommandRegistery.h"

#include "app/commands/AuthCommand.h"
#include "app/commands/ChannelUsersCommand.h"
#include "app/commands/JoinChannelCommand.h"
#include "app/commands/ListCommand.h"
#include "app/commands/SendMessageCommand.h"
#include "app/commands/CreateChannelCommand.h"
#include "app/services/HubPublisher.h"
#include "infra/persistence/chatdb.h"
#include "net/ClientGateway.h"
#include "net/ConnectionManager.h"

namespace app {
void register_all(Dispatcher& d, ChatDB& db, net::ClientGateway& gateway,
                  net::ConnectionManager& connections, services::HubPublisher& hub_pub) {
    d.register_cmd("auth",
                   std::make_unique<AuthCommand>(db, gateway, connections, hub_pub));
    d.register_cmd("list", std::make_unique<ListCommand>(db, connections, hub_pub));
    d.register_cmd("join_channel",
                   std::make_unique<JoinChannelCommand>(db, gateway, connections));
    d.register_cmd("users", std::make_unique<ChannelUsersCommand>(connections));
    d.register_cmd("send_message", std::make_unique<SendMessageCommand>(db, gateway));
    d.register_cmd("create_channel",
                   std::make_unique<CreateChannelCommand>(db, hub_pub));
}
}  // namespace app
