#include "app/commands/hub/LeaveHubCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "domains/Hub.h"
#include "proto/command/hub.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"
#include "proto/event/hub.pb.h"

#include <vector>

namespace app {

std::vector<net::outbound::OutgoingMessage> LeaveHubCommand::execute(CommandContext& ctx,
                                                                     const queue::Event& evt) {
    const auto* event = std::get_if<queue::MessageEvent>(&evt);
    if (!event) {
        return {};
    }

    const auto& env = event->payload.env;
    if (env.type() != sercom::protocol::Envelope::HUB_LEAVE) {
        return {};
    }

    const auto* cmd = get_parsed<sercom::protocol::command::LeaveHub>(*event);
    if (!cmd) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INVALID_FORMAT,
                                   "Invalid HUB_LEAVE payload"));
    }

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!user_exp.has_value()) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
                                   "Authenticate first"));
    }
    const UserId user_id = user_exp.value();

    auto hub_id_opt = ctx.ids.to_internal(PublicHubId{cmd->hub_id()});
    if (!hub_id_opt.has_value()) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_NOT_FOUND,
                                   "Hub not found"));
    }
    const HubId hub_id = hub_id_opt.value();

    auto role = ctx.hub_service.getMembershipRole(hub_id, user_id);
    if (!role.has_value()) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                   "Join the hub before leaving it"));
    }
    if (*role == Role::OWNER) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                   "Owners must transfer ownership before leaving"));
    }

    try {
        ctx.hub_service.removeMember(hub_id, user_id);
    } catch (const std::exception& ex) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
                                   ex.what()));
    } catch (...) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
                                   "Failed to leave hub"));
    }

    const auto channels = ctx.channel_service.getHubChannels(hub_id);
    ctx.subscription_manager.unsubscribe(user_id, Topic::HubTopic(hub_id));
    for (const auto& ch : channels) {
        ctx.subscription_manager.unsubscribe(user_id, Topic::ChannelTopic(hub_id, ch.id));
    }

    const auto public_hub_id = ctx.ids.to_public(hub_id).value;
    const auto public_user_id = ctx.ids.to_public(user_id).value;

    std::vector<GlobalConnId> conns;
    auto subs = ctx.subscription_manager.getSubscribers(Topic::HubTopic(hub_id));
    if (subs.has_value()) {
        conns.reserve(subs->size() + 1);
        for (const auto& uid : subs.value()) {
            auto conn = ctx.session_manager.getMainConnection(uid);
            if (conn.has_value()) conns.push_back(conn.value());
        }
    }

    auto self_conn = ctx.session_manager.getMainConnection(user_id);
    if (self_conn.has_value()) {
        bool exists = false;
        for (const auto& existing : conns) {
            if (existing == self_conn.value()) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            conns.push_back(self_conn.value());
        }
    }

    if (conns.empty()) {
        return {};
    }

    sercom::protocol::event::HubMemberLeft left;
    left.set_hub_id(public_hub_id);
    left.set_user_id(public_user_id);

    sercom::protocol::Envelope out_env;
    out_env.set_version(1);
    out_env.set_type(sercom::protocol::Envelope::HUB_MEMBER_LEFT);
    left.SerializeToString(out_env.mutable_payload());

    std::string bytes;
    out_env.SerializeToString(&bytes);

    return single_outgoing(net::outbound::OutgoingMessage{
        .target = net::outbound::Target::many(std::move(conns)),
        .action =
            net::outbound::Action{std::in_place_type<net::outbound::SendPayload>,
                                  net::outbound::SendPayload{.payload = net::outbound::Payload{
                                      .data = std::move(bytes), .is_binary = true}}}});
}

}  // namespace app
