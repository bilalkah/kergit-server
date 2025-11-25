#include "app/CommandRegistery.h"

#include "app/commands/AuthCommand.h"
// #include "app/commands/CreateChannelCommand.h"
// #include "app/commands/CreateHubCommand.h"
// #include "app/commands/DeleteChannelCommand.h"
// #include "app/commands/DeleteHubCommand.h"
// #include "app/commands/GetHubInviteCommand.h"
// #include "app/commands/JoinChannelCommand.h"
// #include "app/commands/JoinHubByInviteCommand.h"
// #include "app/commands/LeaveHubCommand.h"
// #include "app/commands/RenameChannelCommand.h"
// #include "app/commands/RenameHubCommand.h"
// #include "app/commands/SendMessageCommand.h"
// #include "app/commands/UpdateMemberRoleCommand.h"
// #include "app/commands/UpdateProfileCommand.h"
// #include "app/services/HubPublisher.h"
// #include "app/services/PublicIdService.h"
// #include "infra/persistence/PersistenceGateway.h"
// #include "net/ClientGateway.h"
// #include "net/ConnectionManager.h"

namespace app {
void register_all(Dispatcher& d, PersistenceGateway& db, net::ClientGateway& gateway,
                  net::ConnectionManager& connections, services::HubPublisher& hub_pub,
                  services::PublicIdService& ids) {
    d.register_cmd("auth", std::make_unique<AuthCommand>(db, gateway, connections, hub_pub, ids));
    // d.register_cmd("join_channel",
    //                std::make_unique<JoinChannelCommand>(db, gateway, connections, ids));
    // d.register_cmd("send_message", std::make_unique<SendMessageCommand>(db, gateway, ids));
    // d.register_cmd("create_channel", std::make_unique<CreateChannelCommand>(db, hub_pub, ids));
    // d.register_cmd("delete_channel",
    //                std::make_unique<DeleteChannelCommand>(db, gateway, connections, hub_pub,
    //                ids));
    // d.register_cmd("rename_channel",
    //                std::make_unique<RenameChannelCommand>(db, gateway, hub_pub, ids));
    // d.register_cmd("create_hub", std::make_unique<CreateHubCommand>(db, gateway, hub_pub, ids));
    // d.register_cmd("rename_hub",
    //                std::make_unique<RenameHubCommand>(db, gateway, connections, hub_pub, ids));
    // d.register_cmd("delete_hub",
    //                std::make_unique<DeleteHubCommand>(db, gateway, connections, hub_pub, ids));
    // d.register_cmd("generate_hub_invite", std::make_unique<GetHubInviteCommand>(db, ids));
    // d.register_cmd("join_hub_by_code", std::make_unique<JoinHubByInviteCommand>(
    //                                        db, gateway, connections, hub_pub, ids));
    // d.register_cmd("leave_hub",
    //                std::make_unique<LeaveHubCommand>(db, gateway, connections, hub_pub, ids));
    // d.register_cmd("update_member_role", std::make_unique<UpdateMemberRoleCommand>(
    //                                          db, gateway, connections, ids, hub_pub));
    // d.register_cmd("update_profile", std::make_unique<UpdateProfileCommand>(db, hub_pub, ids));
}
}  // namespace app
