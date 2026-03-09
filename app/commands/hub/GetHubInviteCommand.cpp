#include "app/commands/hub/GetHubInviteCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/proto_builders/EnvelopeBuilders.h"
#include "domains/Hub.h"
#include "proto/command/hub.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"
#include "proto/event/hub.pb.h"

#include <string>
#include <vector>

namespace app {

std::vector<net::outbound::OutgoingMessage> GetHubInviteCommand::execute(CommandContext& ctx,
                                                                         const queue::Event& evt) {
    const auto* event = std::get_if<queue::MessageEvent>(&evt);
    if (!event) {
        return {};
    }

    const auto& env = event->payload.env;
    if (env.type() != sercom::protocol::Envelope::HUB_CREATE_JOIN_CODE) {
        return {};
    }

    const auto& cmd = require_parsed<sercom::protocol::command::CreateHubJoinCode>(*event);

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!user_exp.has_value()) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
            "Authenticate first"));
    }
    const UserId user_id = user_exp.value();

    auto hub_id_opt = parse_wire_id<HubId>(cmd.hub_id());
    if (!hub_id_opt.has_value()) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_NOT_FOUND,
            "Hub not found"));
    }
    const HubId hub_id = hub_id_opt.value();

    if (!ctx.hub_service.isHubMember(hub_id, user_id)) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_FORBIDDEN,
            "Join the hub before requesting a join code"));
    }

    auto role = ctx.hub_service.getMembershipRole(hub_id, user_id);
    if (!role.has_value() || (*role != Role::OWNER && *role != Role::ADMIN)) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_FORBIDDEN,
            "Only owners or admins can create join codes"));
    }

    sercom::protocol::event::HubJoinCodeCreated created;
    created.set_hub_id(hub_id.value);
    created.set_join_code(ctx.invite_service.createInvite(hub_id));

    std::string bytes = proto_builders::serialize_envelope(
        sercom::protocol::Envelope::HUB_JOIN_CODE_CREATED, created);

    return single_outgoing(net::outbound::OutgoingMessage{
        .target = net::outbound::Target::one(event->conn_id),
        .action =
            net::outbound::Action{std::in_place_type<net::outbound::SendPayload>,
                                  net::outbound::SendPayload{
                                      .payload = net::outbound::Payload{std::move(bytes), true}}}});
}

}  // namespace app
