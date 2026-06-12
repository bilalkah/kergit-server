#include "app/commands/activity/KickVoiceParticipantCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/services/hub/RolePolicy.h"
#include "domains/Channel.h"
#include "proto/command/activity.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"

#include <cassert>
#include <vector>

namespace app {

std::vector<net::outbound::OutgoingMessage> KickVoiceParticipantCommand::execute(
    CommandContext& ctx, const queue::Event& evt) {
    std::vector<net::outbound::OutgoingMessage> out;
    assert(std::holds_alternative<queue::MessageEvent>(evt));
    const auto* event = &std::get<queue::MessageEvent>(evt);
    const auto& env = event->payload.env;
    assert(env.type() == sercom::protocol::Envelope::VOICE_KICK_PARTICIPANT);

    const auto& cmd = require_parsed<sercom::protocol::command::KickVoiceParticipant>(*event);
    const auto scope_opt = to_channel_scope(cmd.channel());
    if (!scope_opt.has_value()) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "channel.hub_id and channel.channel_id are required"));
        return out;
    }

    const HubId hub_id = scope_opt->hub_id;
    const ChannelId channel_id = scope_opt->channel_id;
    const UserId target_id{cmd.user_id()};
    if (target_id.value.empty()) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(),
            sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "user_id is required"));
        return out;
    }

    auto actor_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!actor_exp.has_value()) {
        out.emplace_back(make_drop_connection(
            event->conn_id, sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
            "Authenticate first"));
        return out;
    }
    const UserId actor_id = actor_exp.value();

    auto channel_opt = ctx.hub_service.getChannel(channel_id);
    if (!channel_opt || channel_opt->hub_id != hub_id) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_NOT_FOUND,
            "Channel not found"));
        return out;
    }
    if (channel_opt->type != ChannelType::VOICE) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "Channel is not a voice channel"));
        return out;
    }

    auto actor_role = ctx.hub_service.getMembershipRole(hub_id, actor_id);
    if (!actor_role.has_value()) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_FORBIDDEN,
            "Join the hub before moderating voice"));
        return out;
    }

    auto target_role = ctx.hub_service.getMembershipRole(hub_id, target_id);
    if (!target_role.has_value()) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_NOT_FOUND,
            "Target user is not a member"));
        return out;
    }

    if (!services::hub::canKickVoiceParticipant(*actor_role, *target_role, actor_id, target_id)) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_FORBIDDEN,
            "You are not allowed to kick this participant"));
        return out;
    }

    const auto target_channel = ctx.voice_service.sessions().user_channel(target_id);
    if (!target_channel.has_value() || *target_channel != channel_id) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(),
            sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "Target user is not connected to this voice channel"));
        return out;
    }

    if (!ctx.voice_service.verified_kick_user(channel_id, target_id)) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
            "Failed to remove user from voice channel"));
    }

    return out;
}

}  // namespace app
