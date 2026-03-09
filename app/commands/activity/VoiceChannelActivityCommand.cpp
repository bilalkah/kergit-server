#include "app/commands/activity/VoiceChannelActivityCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "app/proto_builders/EnvelopeBuilders.h"
#include "domains/Channel.h"
#include "proto/command/activity.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/activity.pb.h"
#include "proto/event/error.pb.h"

#include <vector>

namespace app {

std::vector<net::outbound::OutgoingMessage> VoiceChannelActivityCommand::execute(
    CommandContext& ctx, const queue::Event& evt) {
    const auto* event = std::get_if<queue::MessageEvent>(&evt);
    if (!event) return {};

    const auto& env = event->payload.env;
    if (env.type() != sercom::protocol::Envelope::VOICE_ACTIVITY) {
        return {make_drop_connection(event->conn_id,
                                     sercom::protocol::event::CommandErrorCode_INVALID_FORMAT,
                                     "Invalid VOICE_ACTIVITY envelope type")};
    }

    const auto& cmd = require_parsed<sercom::protocol::command::VoiceChannelActivity>(*event);

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!user_exp.has_value()) {
        return {make_drop_connection(event->conn_id,
                                     sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
                                     "Authenticate first")};
    }
    const UserId user_id = user_exp.value();

    auto requester_session_id_exp = ctx.session_manager.sessionIdOfConnection(event->conn_id);
    if (!requester_session_id_exp.has_value()) {
        return {make_drop_connection(event->conn_id,
                                     sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
                                     "Runtime session missing")};
    }
    const SessionId requester_session_id = requester_session_id_exp.value();

    auto channel_id_opt = parse_wire_id<ChannelId>(cmd.channel_id());
    if (!channel_id_opt.has_value()) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_NOT_FOUND,
            "Channel not found"));
    }

    auto channel_opt = ctx.channel_service.getChannel(*channel_id_opt);
    if (!channel_opt) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_NOT_FOUND,
            "Channel not found"));
    }
    const HubId hub_id = channel_opt->hub_id;

    if (channel_opt->type != ChannelType::VOICE) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "Channel is not a voice channel"));
    }

    if (!ctx.hub_service.isHubMember(hub_id, user_id)) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_FORBIDDEN,
            "Join the hub before updating voice state"));
    }

    auto& voice_sessions = ctx.voice_service.sessions();
    const auto current_channel = voice_sessions.user_channel(user_id);
    const auto owner_session = voice_sessions.user_session(user_id);

    if (!current_channel.has_value()) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "Join a voice channel first"));
    }

    if (owner_session.has_value() && *owner_session != requester_session_id) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_FORBIDDEN,
            "Voice is connected from another session"));
    }

    if (*current_channel != *channel_id_opt) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "Voice activity must target the active voice channel"));
    }

    bool changed = false;
    auto state = cmd.state();
    if (state == sercom::protocol::command::VoiceChannelActivity::STATE_MUTE) {
        changed = voice_sessions.set_muted(user_id, true);
    } else if (state == sercom::protocol::command::VoiceChannelActivity::STATE_UNMUTE) {
        changed = voice_sessions.set_muted(user_id, false);
    } else if (state == sercom::protocol::command::VoiceChannelActivity::STATE_DEAFEN) {
        changed = voice_sessions.set_deafened(user_id, true);
    } else if (state == sercom::protocol::command::VoiceChannelActivity::STATE_UNDEAFEN) {
        changed = voice_sessions.set_deafened(user_id, false);
    } else {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "Voice activity state is unspecified"));
    }

    if (!changed) return {};

    utils::metrics::counters().fanout_subscriber_snapshot_total.fetch_add(
        1, std::memory_order_relaxed);
    auto subs = ctx.subscription_manager.getSubscribers(Topic::HubTopic(hub_id));
    if (!subs || subs->empty()) return {};

    std::vector<GlobalConnId> conns(subs->begin(), subs->end());
    if (conns.empty()) return {};

    sercom::protocol::event::VoiceChannelParticipants participants;
    participants.set_channel_id(channel_id_opt->value);
    for (const auto& p : voice_sessions.participants_in_channel(*channel_id_opt)) {
        auto* out_p = participants.add_participants();
        out_p->set_user_id(p.user_id.value);
        out_p->set_muted(p.muted);
        out_p->set_deafened(p.deafened);
    }

    std::string bytes = proto_builders::serialize_envelope(
        sercom::protocol::Envelope::VOICE_CHANNEL_PARTICIPANTS, participants);

    return single_outgoing(net::outbound::OutgoingMessage{
        .priority = net::outbound::OutboundPriority::Low,
        .target = net::outbound::Target::many(std::move(conns)),
        .action =
            net::outbound::Action{std::in_place_type<net::outbound::SendPayload>,
                                  net::outbound::SendPayload{
                                      .payload = net::outbound::Payload{std::move(bytes), true}}}});
}

}  // namespace app
