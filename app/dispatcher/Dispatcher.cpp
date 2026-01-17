#include "app/dispatcher/Dispatcher.h"

// #include "app/commands/channel/CreateChannelCommand.h"
// #include "app/commands/channel/DeleteChannelCommand.h"
// #include "app/commands/channel/JoinChannelCommand.h"
// #include "app/commands/channel/RenameChannelCommand.h"
// #include "app/commands/hub/CreateHubCommand.h"
// #include "app/commands/hub/DeleteHubCommand.h"
// #include "app/commands/hub/GetHubInviteCommand.h"
// #include "app/commands/hub/JoinHubByInviteCommand.h"
// #include "app/commands/hub/LeaveHubCommand.h"
// #include "app/commands/hub/RenameHubCommand.h"
// #include "app/commands/member/UpdateMemberRoleCommand.h"
// #include "app/commands/message/SendMessageCommand.h"
// #include "app/commands/profile/UpdateProfileCommand.h"
#include "app/commands/session/AuthenticateCommand.h"
#include "app/commands/session/BootstrapCommand.h"
#include "app/commands/system/DisconnectionCommand.h"

namespace app {

std::vector<net::outbound::OutgoingMessage> Dispatcher::dispatch(const std::string& type,
                                                                 CommandContext& ctx,
                                                                 const queue::Event& evt) {
    auto it = map_str_.find(type);
    if (it == map_str_.end()) {
        log(utils::LogLevel::WARN, "No command registered for type '", type, "'");
        return {};
    }
    return it->second->execute(ctx, evt);
}

std::vector<net::outbound::OutgoingMessage> Dispatcher::dispatch(
    const sercom::protocol::Envelope_Type type, CommandContext& ctx, const queue::Event& evt) {
    auto it = map_proto_.find(type);
    if (it == map_proto_.end()) {
        log(utils::LogLevel::WARN, "No command registered for proto type '", static_cast<int>(type),
            "'");
        return {};
    }
    return it->second->execute(ctx, evt);
}

std::unordered_set<std::string> Dispatcher::registered_commands() const {
    std::unordered_set<std::string> commands;
    for (const auto& pair : map_str_) {
        commands.insert(pair.first);
    }
    return commands;
}

void Dispatcher::register_all() {
    map_str_["connection"] = std::make_unique<BootstrapCommand>();
    map_str_["disconnection"] = std::make_unique<DisconnectionCommand>();

    map_proto_[sercom::protocol::Envelope_Type_AUTH] = std::make_unique<AuthenticateCommand>();
    // register_cmd("join_channel", std::make_unique<JoinChannelCommand>());
    // register_cmd("send_message", std::make_unique<SendMessageCommand>());
    // register_cmd("create_channel", std::make_unique<CreateChannelCommand>());
    // register_cmd("delete_channel", std::make_unique<DeleteChannelCommand>());
    // register_cmd("rename_channel", std::make_unique<RenameChannelCommand>());
    // register_cmd("delete_hub", std::make_unique<DeleteHubCommand>());
    // register_cmd("create_hub", std::make_unique<CreateHubCommand>());
    // register_cmd("rename_hub", std::make_unique<RenameHubCommand>());
    // register_cmd("generate_hub_invite", std::make_unique<GetHubInviteCommand>());
    // register_cmd("join_hub_by_code", std::make_unique<JoinHubByInviteCommand>());
    // register_cmd("leave_hub", std::make_unique<LeaveHubCommand>());
    // register_cmd("update_member_role", std::make_unique<UpdateMemberRoleCommand>());
    // register_cmd("update_profile", std::make_unique<UpdateProfileCommand>());
}

}  // namespace app
