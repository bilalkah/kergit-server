#include "app/commands/activity/JoinVoiceChannelCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "app/proto_builders/EnvelopeBuilders.h"
#include "app/proto_builders/VoiceBuilders.h"
#include "domains/Channel.h"
#include "proto/command/activity.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/activity.pb.h"
#include "proto/event/error.pb.h"
#include "utils/EventLogger.h"

#include <expected>
#include <string>
#include <vector>

namespace app {
namespace {

net::outbound::OutgoingMessage make_broadcast(std::vector<GlobalConnId> conns, std::string bytes) {
    return net::outbound::OutgoingMessage{
        .priority = net::outbound::OutboundPriority::Low,
        .target = net::outbound::Target::many(std::move(conns)),
        .action = net::outbound::Action{
            std::in_place_type<net::outbound::SendPayload>,
            net::outbound::SendPayload{
                .payload = net::outbound::Payload{std::move(bytes), true}}}};
}

std::vector<GlobalConnId> hub_subscriber_conns(CommandContext& ctx, const HubId& hub) {
    utils::metrics::counters().fanout_subscriber_snapshot_total.fetch_add(
        1, std::memory_order_relaxed);
    auto subs = ctx.subscription_manager.getSubscribers(Topic::HubTopic(hub));
    if (!subs || subs->empty()) return {};
    return {subs->begin(), subs->end()};
}

void append(std::vector<net::outbound::OutgoingMessage>& out,
            std::vector<net::outbound::OutgoingMessage> msgs) {
    out.insert(out.end(), std::make_move_iterator(msgs.begin()),
               std::make_move_iterator(msgs.end()));
}

}  // namespace

std::vector<net::outbound::OutgoingMessage> JoinVoiceChannelCommand::execute(
    CommandContext& ctx, const queue::Event& evt) {
    const auto* event = std::get_if<queue::MessageEvent>(&evt);
    if (!event) return {};

    const auto& env = event->payload.env;
    if (env.type() != sercom::protocol::Envelope::VOICE_JOIN) {
        return {make_drop_connection(event->conn_id,
                                     sercom::protocol::event::CommandErrorCode_INVALID_FORMAT,
                                     "Invalid VOICE_JOIN envelope type")};
    }

    const auto& cmd = require_parsed<sercom::protocol::command::VoiceChannelMembership>(*event);
    auto channel_id_opt = parse_wire_id<ChannelId>(cmd.channel_id());
    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);

    utils::EventLogger::instance().log(
        utils::EventCategory::VOICE, user_exp.has_value() ? user_exp->value : "",
        "VOICE_MEMBERSHIP_REQUEST", 0,
        "conn:" + event->conn_id.netstack_id.value + ":" + event->conn_id.conn_id.value +
            " channel:" + (channel_id_opt.has_value() ? channel_id_opt->value : "unknown") +
            " state:" + sercom::protocol::command::VoiceChannelMembership_State_Name(cmd.state()),
        utils::LogDest::BOTH);

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
            event->conn_id, env.type(),
            sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "Channel is not a voice channel"));
    }

    if (!ctx.hub_service.isHubMember(hub_id, user_id)) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_FORBIDDEN,
            "Join the hub before joining voice"));
    }

    auto& voice_sessions = ctx.voice_service.sessions();

    // --- helpers ---

    auto publish_participants = [&](const HubId& hub, const ChannelId& channel) {
        auto conns = hub_subscriber_conns(ctx, hub);
        if (conns.empty()) return std::vector<net::outbound::OutgoingMessage>{};

        sercom::protocol::event::VoiceChannelParticipants participants;
        participants.set_channel_id(channel.value);
        for (const auto& p : voice_sessions.participants_in_channel(channel)) {
            auto* out_p = participants.add_participants();
            out_p->set_user_id(p.user_id.value);
            out_p->set_muted(p.muted);
            out_p->set_deafened(p.deafened);
        }
        return single_outgoing(make_broadcast(
            std::move(conns),
            proto_builders::serialize_envelope(
                sercom::protocol::Envelope::VOICE_CHANNEL_PARTICIPANTS, participants)));
    };

    auto publish_presence = [&](const HubId& hub, const ChannelId& channel,
                                sercom::protocol::event::VoiceChannelActivityState state) {
        auto conns = hub_subscriber_conns(ctx, hub);
        if (conns.empty()) return std::vector<net::outbound::OutgoingMessage>{};

        auto presence =
            proto_builders::voice::make_voice_presence(channel.value, user_id.value, state);
        return single_outgoing(make_broadcast(
            std::move(conns),
            proto_builders::serialize_envelope(
                sercom::protocol::Envelope::VOICE_CHANNEL_PRESENCE, presence)));
    };

    auto publish_self_status = [&](bool connected,
                                   const std::optional<SessionId>& owner_session_id,
                                   const std::optional<SessionId>& skip_session_id = std::nullopt) {
        std::vector<net::outbound::OutgoingMessage> out_msgs;

        // Look up current voice state for the user.
        auto current_ch = voice_sessions.user_channel(user_id);
        bool self_muted = false;
        bool self_deafened = false;
        if (connected && current_ch.has_value()) {
            for (const auto& p : voice_sessions.participants_in_channel(*current_ch)) {
                if (p.user_id == user_id) {
                    self_muted = p.muted;
                    self_deafened = p.deafened;
                    break;
                }
            }
        }

        for (const auto& session_id : ctx.session_manager.getUserSessionIds(user_id)) {
            if (skip_session_id.has_value() && session_id == *skip_session_id) continue;
            auto session_conns = ctx.session_manager.getSessionIdConnections(session_id);
            if (session_conns.empty()) continue;

            sercom::protocol::event::VoiceSelfStatus status;
            status.set_connected(connected);
            status.set_is_owner(connected && owner_session_id.has_value() &&
                                owner_session_id.value() == session_id);
            if (connected && current_ch.has_value()) {
                status.set_channel_id(current_ch->value);
                status.set_muted(self_muted);
                status.set_deafened(self_deafened);
            }
            out_msgs.push_back(make_broadcast(
                std::move(session_conns),
                proto_builders::serialize_envelope(
                    sercom::protocol::Envelope::VOICE_SELF_STATUS, status)));
        }
        return out_msgs;
    };

    // --- STATE_JOIN: issue token, auto-takeover if needed ---

    if (cmd.state() == sercom::protocol::command::VoiceChannelMembership::STATE_JOIN) {
        std::vector<net::outbound::OutgoingMessage> out;

        // Check if another session owns voice — if so, revoke it automatically.
        const auto old_owner_session = voice_sessions.user_session(user_id);
        const bool taking_over =
            old_owner_session.has_value() && *old_owner_session != requester_session_id;
        if (taking_over) {
            const auto old_conns =
                ctx.session_manager.getSessionIdConnections(*old_owner_session);
            utils::EventLogger::instance().log(
                utils::EventCategory::VOICE, user_id.value, "voice_takeover", 0,
                "old_session=" + std::to_string(*old_owner_session) +
                    " new_session=" + std::to_string(requester_session_id),
                utils::LogDest::BOTH);

            // If the user is the sole participant in their current channel,
            // guard against ROOM_FINISHED arriving before the new session connects.
            auto old_channel = voice_sessions.user_channel(user_id);
            if (old_channel.has_value() &&
                voice_sessions.participants_in_channel(*old_channel).size() == 1) {
                ctx.voice_service.mark_channel_takeover(*old_channel);
            }

            if (!old_conns.empty()) {
                sercom::protocol::event::VoiceSelfRevoked revoked;
                out.push_back(make_broadcast(
                    old_conns,
                    proto_builders::serialize_envelope(
                        sercom::protocol::Envelope::VOICE_SELF_REVOKED, revoked)));
            }
        }

        // Mint the token.
        sercom::protocol::event::VoiceTokenIssued issued;
        try {
            issued = ctx.voice_service.join_voice(*channel_id_opt, user_id);
        } catch (const std::exception& ex) {
            return single_outgoing(make_command_error(
                event->conn_id, env.type(),
                sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR, ex.what()));
        }

        if (issued.token().empty() || issued.livekit_url().empty()) {
            return single_outgoing(make_command_error(
                event->conn_id, env.type(),
                sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
                "Voice token issuance failed"));
        }

        utils::EventLogger::instance().log(
            utils::EventCategory::VOICE, user_id.value, "voice_token_issued", 0,
            "session=" + std::to_string(requester_session_id) +
                " channel=" + channel_id_opt->value +
                " token_len=" + std::to_string(issued.token().size()) +
                " livekit_url=" + issued.livekit_url(),
            utils::LogDest::BOTH);

        if (taking_over) {
            ctx.voice_service.mark_takeover(user_id);
        }

        // Capture old channel before join (for channel-switch cleanup).
        const auto old_channel_before_join = voice_sessions.user_channel(user_id);

        // Register in VoiceSessionManager (handles channel-switch cleanup internally).
        voice_sessions.join(*channel_id_opt, user_id, requester_session_id);

        // Persist to DB (applies any recovery preferences and upserts).
        ctx.voice_service.persist_voice_join(user_id, *channel_id_opt);

        // Send token to requester.
        std::string token_bytes = proto_builders::serialize_envelope(
            sercom::protocol::Envelope::VOICE_TOKEN_ISSUED, issued);
        out.push_back(net::outbound::OutgoingMessage{
            .target = net::outbound::Target::one(event->conn_id),
            .action = net::outbound::Action{
                std::in_place_type<net::outbound::SendPayload>,
                net::outbound::SendPayload{
                    .payload = net::outbound::Payload{std::move(token_bytes), true}}}});

        // If switching channels, broadcast cleanup for the old channel.
        if (old_channel_before_join.has_value() && *old_channel_before_join != *channel_id_opt) {
            if (auto old_ch = ctx.channel_service.getChannel(*old_channel_before_join)) {
                append(out, publish_participants(old_ch->hub_id, *old_channel_before_join));
                append(out, publish_presence(old_ch->hub_id, *old_channel_before_join,
                                             sercom::protocol::event::ACTIVITY_LEFT));
            }
            if (voice_sessions.is_empty(*old_channel_before_join)) {
                ctx.voice_service.on_channel_empty(*old_channel_before_join);
            }
        }

        // Broadcast updated participants + presence + self status for new channel.
        append(out, publish_participants(hub_id, *channel_id_opt));
        append(out, publish_presence(hub_id, *channel_id_opt,
                                     sercom::protocol::event::ACTIVITY_JOINED));
        // Skip sending VoiceSelfStatus to the old owner session — it gets VOICE_SELF_REVOKED instead.
        append(out, publish_self_status(true, requester_session_id,
                                        taking_over ? old_owner_session : std::nullopt));
        return out;
    }

    // --- STATE_LEAVE: update state immediately, then kick from LiveKit ---

    if (cmd.state() == sercom::protocol::command::VoiceChannelMembership::STATE_LEAVE) {
        const auto leave_result = voice_sessions.leave_if_owner(user_id, requester_session_id);
        if (!leave_result.removed) {
            if (!voice_sessions.user_channel(user_id).has_value()) {
                return {};  // Not in voice, nothing to do.
            }
            return single_outgoing(make_command_error(
                event->conn_id, env.type(),
                sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                "Voice is connected from another session"));
        }

        const ChannelId current_channel = *leave_result.channel;

        // Persist leave to DB.
        ctx.voice_service.persist_voice_leave(user_id);

        // Kick from LiveKit (best-effort; server state was already updated above).
        ctx.voice_service.kick_user(current_channel, user_id);

        if (leave_result.became_empty) {
            ctx.voice_service.on_channel_empty(current_channel);
        }

        std::vector<net::outbound::OutgoingMessage> out;

        // Look up the hub for the channel we're leaving.
        if (auto leaving_ch = ctx.channel_service.getChannel(current_channel)) {
            append(out, publish_participants(leaving_ch->hub_id, current_channel));
            append(out, publish_presence(leaving_ch->hub_id, current_channel,
                                         sercom::protocol::event::ACTIVITY_LEFT));
        }
        append(out, publish_self_status(false, std::nullopt));
        return out;
    }

    return single_outgoing(make_command_error(
        event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
        "Unsupported voice membership state"));
}

}  // namespace app
