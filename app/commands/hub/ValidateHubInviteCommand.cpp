#include "app/commands/hub/ValidateHubInviteCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "domains/Hub.h"
#include "proto/command/hub.pb.h"
#include "proto/domain/hub.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"

#include <cassert>
#include <string>
#include <vector>

namespace app {

std::vector<net::outbound::OutgoingMessage> ValidateHubInviteCommand::execute(
    CommandContext& ctx, const queue::Event& evt) {
    std::vector<net::outbound::OutgoingMessage> out;
    assert(std::holds_alternative<queue::MessageEvent>(evt));
    const auto* event = &std::get<queue::MessageEvent>(evt);
    const auto& env = event->payload.env;
    assert(env.type() == sercom::protocol::Envelope::HUB_INVITE_VALIDATE);

    const auto& cmd = require_parsed<sercom::protocol::command::ValidateHubInvite>(*event);

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!user_exp.has_value()) {
        out.emplace_back(make_command_error(event->conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
                                            "Authenticate first"));
        return out;
    }
    const UserId user_id = user_exp.value();

    // Accept either a bare token or a full /invite/<token> link.
    std::string raw_code = cmd.join_code();
    const auto invite_pos = raw_code.find("/invite/");
    const std::string token =
        (invite_pos != std::string::npos) ? raw_code.substr(invite_pos + 8) : raw_code;

    auto hub_id_opt = ctx.invite_service.resolveInvite(token);
    if (!hub_id_opt.has_value()) {
        ctx.audit_service.log(AuditRepository::Event{
            .category = "hub",
            .event_type = "hub.invite.expired",
            .severity = "info",
            .actor_type = "user",
            .actor_user_id = user_id,
            .session_id = std::to_string(
                ctx.session_manager.sessionIdOfConnection(event->conn_id).value_or(0)),
            .connection_id = to_string(event->conn_id),
            .metadata =
                nlohmann::json{
                    {"code", token},
                },
        });

        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVITE_EXPIRED,
            "Invite link is invalid or has expired"));
        return out;
    }

    auto hub = ctx.hub_service.getHub(hub_id_opt.value());
    if (!hub.has_value()) {
        out.emplace_back(make_command_error(event->conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_NOT_FOUND,
                                            "Hub not found"));
        return out;
    }

    // Valid invite: reply with a hub preview under the same envelope type.
    sercom::protocol::Envelope reply;
    reply.set_version(1);
    reply.set_type(sercom::protocol::Envelope::HUB_INVITE_VALIDATE);
    to_proto_hub(*hub).SerializeToString(reply.mutable_payload());

    auto self_conns = ctx.session_manager.getSessionConnections(user_id);
    if (self_conns.empty()) {
        self_conns.push_back(event->conn_id);
    }

    out.emplace_back(make_outgoing_message(net::outbound::Target::many(std::move(self_conns)),
                                           reply.SerializeAsString()));
    return out;
}

}  // namespace app
