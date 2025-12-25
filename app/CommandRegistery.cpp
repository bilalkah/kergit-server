#include "app/CommandRegistery.h"

#include "app/commands/AuthCommand.h"
#include "app/commands/CreateChannelCommand.h"
#include "app/commands/CreateHubCommand.h"
#include "app/commands/DeleteChannelCommand.h"
#include "app/commands/DeleteHubCommand.h"
#include "app/commands/GetHubInviteCommand.h"
#include "app/commands/JoinChannelCommand.h"
#include "app/commands/JoinHubByInviteCommand.h"
#include "app/commands/LeaveHubCommand.h"
#include "app/commands/RenameChannelCommand.h"
#include "app/commands/RenameHubCommand.h"
#include "app/commands/SendMessageCommand.h"
#include "app/commands/UpdateMemberRoleCommand.h"
#include "app/commands/UpdateProfileCommand.h"
#include "app/commands/system/DisconnectCommand.h"

namespace app {
void register_all(Dispatcher& d, ServiceObjects& svc_objs) {
    d.register_cmd("auth", std::make_unique<AuthCommand>(svc_objs));
    d.register_cmd("join_channel", std::make_unique<JoinChannelCommand>(svc_objs));
    d.register_cmd("send_message", std::make_unique<SendMessageCommand>(svc_objs));
    d.register_cmd("create_channel", std::make_unique<CreateChannelCommand>(svc_objs));
    d.register_cmd("delete_channel", std::make_unique<DeleteChannelCommand>(svc_objs));
    d.register_cmd("rename_channel", std::make_unique<RenameChannelCommand>(svc_objs));
    d.register_cmd("create_hub", std::make_unique<CreateHubCommand>(svc_objs));
    d.register_cmd("rename_hub", std::make_unique<RenameHubCommand>(svc_objs));
    d.register_cmd("delete_hub", std::make_unique<DeleteHubCommand>(svc_objs));
    d.register_cmd("generate_hub_invite", std::make_unique<GetHubInviteCommand>(svc_objs));
    d.register_cmd("join_hub_by_code", std::make_unique<JoinHubByInviteCommand>(svc_objs));
    d.register_cmd("leave_hub", std::make_unique<LeaveHubCommand>(svc_objs));
    d.register_cmd("update_member_role", std::make_unique<UpdateMemberRoleCommand>(svc_objs));
    d.register_cmd("update_profile", std::make_unique<UpdateProfileCommand>(svc_objs));
    d.register_cmd("disconnect", std::make_unique<DisconnectCommand>(svc_objs));
}
}  // namespace app
