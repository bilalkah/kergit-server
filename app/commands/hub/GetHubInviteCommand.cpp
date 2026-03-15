#include "app/commands/hub/GetHubInviteCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "domains/Hub.h"
#include "proto/command/hub.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"
#include "proto/event/state.pb.h"

#include <cassert>
#include <string>
#include <vector>

namespace app {

std::vector<net::outbound::OutgoingMessage> GetHubInviteCommand::execute(CommandContext& ctx,
                                                                         const queue::Event& evt) {
    std::vector<net::outbound::OutgoingMessage> out;
    assert(std::holds_alternative<queue::MessageEvent>(evt));
    const auto* event = &std::get<queue::MessageEvent>(evt);
    const auto& env = event->payload.env;
    assert(env.type() == sercom::protocol::Envelope::HUB_CREATE_JOIN_CODE);

    const auto& cmd = require_parsed<sercom::protocol::command::CreateHubJoinCode>(*event);

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!user_exp.has_value()) {
        out.emplace_back(make_drop_connection(event->conn_id,
                                              sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                              "Authenticate before sending messages"));
        return out;
    }

    const UserId user_id = user_exp.value();
    const HubId hub_id{cmd.hub_id()};
    auto role = ctx.hub_service.getMembershipRole(hub_id, user_id);
    if (!role) {
        out.emplace_back(make_command_error(event->conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                            "Join the hub before requesting a join code"));
        return out;
    }

    if (*role != Role::OWNER && *role != Role::ADMIN) {
        out.emplace_back(make_command_error(event->conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                            "Only owners or admins can create join codes"));
        return out;
    }

    const std::string join_code = ctx.invite_service.createInvite(hub_id);

    sercom::protocol::event::StateDelta delta;
    auto* hub_delta = delta.add_hubs();
    hub_delta->set_hub_id(hub_id.value);
    hub_delta->add_hub_ops()->mutable_join_code_set()->set_join_code(join_code);

    out.emplace_back(make_outgoing_message(net::outbound::Target::one(event->conn_id),
                                           make_state_delta(delta)));
    return out;
}

}  // namespace app
