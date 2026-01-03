#include "app/dispatcher/Dispatcher.h"

#include "app/commands/auth/AuthCommand.h"
#include "app/commands/channel/CreateChannelCommand.h"
#include "app/commands/channel/DeleteChannelCommand.h"
#include "app/commands/channel/JoinChannelCommand.h"
#include "app/commands/channel/RenameChannelCommand.h"
#include "app/commands/hub/DeleteHubCommand.h"
#include "app/commands/hub/CreateHubCommand.h"
#include "app/commands/hub/RenameHubCommand.h"
#include "app/commands/hub/GetHubInviteCommand.h"
#include "app/commands/message/SendMessageCommand.h"
#include "app/commands/system/DisconnectionCommand.h"
#include "app/commands/hub/JoinHubByInviteCommand.h"
#include "app/commands/hub/LeaveHubCommand.h"

namespace app {

void Dispatcher::register_cmd(std::string type, std::unique_ptr<ICommand> cmd) {
    map_[std::move(type)] = std::move(cmd);
}

CommandResult Dispatcher::dispatch(const std::string& type, CommandContext& ctx,
                                   const CommandInput cmd) {
    auto it = map_.find(type);
    if (it == map_.end()) {
        return std::unexpected(CommandError{"unknown_command", "Unknown command type: " + type});
    }
    return it->second->execute(ctx, cmd);
}

std::unordered_set<std::string> Dispatcher::registered_commands() const {
    std::unordered_set<std::string> commands;
    for (const auto& pair : map_) {
        commands.insert(pair.first);
    }
    return commands;
}

void Dispatcher::register_all() {
    register_cmd("auth", std::make_unique<AuthCommand>());
    register_cmd("disconnection", std::make_unique<DisconnectionCommand>());
    register_cmd("join_channel", std::make_unique<JoinChannelCommand>());
    register_cmd("send_message", std::make_unique<SendMessageCommand>());
    register_cmd("create_channel", std::make_unique<CreateChannelCommand>());
    register_cmd("delete_channel", std::make_unique<DeleteChannelCommand>());
    register_cmd("rename_channel", std::make_unique<RenameChannelCommand>());
    register_cmd("delete_hub", std::make_unique<DeleteHubCommand>());
    register_cmd("create_hub", std::make_unique<CreateHubCommand>());
    register_cmd("rename_hub", std::make_unique<RenameHubCommand>());
    register_cmd("generate_hub_invite", std::make_unique<GetHubInviteCommand>());
    register_cmd("join_hub_by_code", std::make_unique<JoinHubByInviteCommand>());
    register_cmd("leave_hub", std::make_unique<LeaveHubCommand>());
    // register_cmd("delete_channel", std::make_unique<DeleteChannelCommand>());
    // register_cmd("create_hub", std::make_unique<CreateHubCommand>());
    // register_cmd("rename_hub", std::make_unique<RenameHubCommand>());
    // register_cmd("delete_hub", std::make_unique<DeleteHubCommand>());
    // register_cmd("generate_hub_invite", std::make_unique<GetHubInviteCommand>());
    // register_cmd("join_hub_by_code", std::make_unique<JoinHubByInviteCommand>());
    // register_cmd("leave_hub", std::make_unique<LeaveHubCommand>());
    // register_cmd("update_member_role", std::make_unique<UpdateMemberRoleCommand>());
    // register_cmd("update_profile", std::make_unique<UpdateProfileCommand>());
    // register_cmd("disconnect", std::make_unique<DisconnectCommand>());
}

}  // namespace app
