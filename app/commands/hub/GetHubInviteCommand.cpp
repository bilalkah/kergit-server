#include "app/commands/hub/GetHubInviteCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "domains/Hub.h"
#include "proto/command/hub.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"
#include "proto/event/hub.pb.h"

#include <cassert>
#include <string>
#include <vector>

namespace app {

std::string make_hub_join_code_created(const HubId& hub_id, std::string join_code) {
    sercom::protocol::event::HubJoinCodeCreated created;
    created.set_hub_id(hub_id.value);
    created.set_join_code(std::move(join_code));

    sercom::protocol::Envelope out_env;
    out_env.set_version(1);
    out_env.set_type(sercom::protocol::Envelope::HUB_JOIN_CODE_CREATED);
    created.SerializeToString(out_env.mutable_payload());
    return out_env.SerializeAsString();
}

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
    if (!ctx.hub_service.isHubMember(hub_id, user_id)) {
        out.emplace_back(make_command_error(event->conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                            "Join the hub before requesting a join code"));
        return out;
    }

    auto role = ctx.hub_service.getMembershipRole(hub_id, user_id);
    if (!role.has_value() || (*role != Role::OWNER && *role != Role::ADMIN)) {
        out.emplace_back(make_command_error(event->conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                            "Only owners or admins can create join codes"));
        return out;
    }

    std::string bytes = make_hub_join_code_created(hub_id, ctx.invite_service.createInvite(hub_id));

    out.emplace_back(
        make_outgoing_message(net::outbound::Target::one(event->conn_id), std::move(bytes)));
    return out;
}

}  // namespace app
