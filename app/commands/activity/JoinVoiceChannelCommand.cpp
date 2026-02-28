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

#include <chrono>
#include <expected>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace app {
namespace {

std::string format_connection_id(const GlobalConnId& conn) {
    return conn.netstack_id.value + ":" + conn.conn_id.value;
}

std::string join_connection_ids(const std::vector<GlobalConnId>& conns) {
    std::ostringstream oss;
    for (std::size_t i = 0; i < conns.size(); ++i) {
        if (i > 0) {
            oss << "|";
        }
        oss << format_connection_id(conns[i]);
    }
    return oss.str();
}

}  // namespace

std::vector<net::outbound::OutgoingMessage> JoinVoiceChannelCommand::execute(
    CommandContext& ctx, const queue::Event& evt) {
    const auto* event = std::get_if<queue::MessageEvent>(&evt);
    if (!event) {
        return {};
    }

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
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "Channel is not a voice channel"));
    }

    if (!ctx.hub_service.isHubMember(hub_id, user_id)) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_FORBIDDEN,
            "Join the hub before joining voice"));
    }

    auto publish_participants = [&](const HubId& hub, const ChannelId& channel) {
        utils::metrics::counters().fanout_subscriber_snapshot_total.fetch_add(
            1, std::memory_order_relaxed);
        auto subs = ctx.subscription_manager.getSubscribers(Topic::HubTopic(hub));
        if (!subs || subs->empty()) {
            return std::vector<net::outbound::OutgoingMessage>{};
        }

        std::vector<GlobalConnId> conns;
        conns.reserve(subs->size());
        for (const auto& conn : *subs) {
            conns.push_back(conn);
        }
        if (conns.empty()) {
            return std::vector<net::outbound::OutgoingMessage>{};
        }

        sercom::protocol::event::VoiceChannelParticipants participants;
        participants.set_channel_id(channel.value);

        const auto users = ctx.session_manager.voiceParticipantStatesInChannel(channel);
        for (const auto& user : users) {
            auto* participant = participants.add_participants();
            participant->set_user_id(user.user_id.value);
            participant->set_muted(user.muted);
            participant->set_deafened(user.deafened);
        }
        std::string bytes = proto_builders::serialize_envelope(
            sercom::protocol::Envelope::VOICE_CHANNEL_PARTICIPANTS, participants);

        return single_outgoing(net::outbound::OutgoingMessage{
            .priority = net::outbound::OutboundPriority::Low,
            .target = net::outbound::Target::many(std::move(conns)),
            .action =
                net::outbound::Action{std::in_place_type<net::outbound::SendPayload>,
                                      net::outbound::SendPayload{.payload = net::outbound::Payload{
                                                                     std::move(bytes), true}}}});
    };

    auto publish_presence = [&](const HubId& hub, const ChannelId& channel,
                                sercom::protocol::event::VoiceChannelActivityState state) {
        utils::metrics::counters().fanout_subscriber_snapshot_total.fetch_add(
            1, std::memory_order_relaxed);
        auto subs = ctx.subscription_manager.getSubscribers(Topic::HubTopic(hub));
        if (!subs || subs->empty()) {
            return std::vector<net::outbound::OutgoingMessage>{};
        }

        std::vector<GlobalConnId> conns;
        conns.reserve(subs->size());
        for (const auto& conn : *subs) {
            conns.push_back(conn);
        }
        if (conns.empty()) {
            return std::vector<net::outbound::OutgoingMessage>{};
        }

        auto presence =
            proto_builders::voice::make_voice_presence(channel.value, user_id.value, state);
        std::string bytes = proto_builders::serialize_envelope(
            sercom::protocol::Envelope::VOICE_CHANNEL_PRESENCE, presence);

        return single_outgoing(net::outbound::OutgoingMessage{
            .priority = net::outbound::OutboundPriority::Low,
            .target = net::outbound::Target::many(std::move(conns)),
            .action =
                net::outbound::Action{std::in_place_type<net::outbound::SendPayload>,
                                      net::outbound::SendPayload{.payload = net::outbound::Payload{
                                                                     std::move(bytes), true}}}});
    };

    auto publish_self_status = [&](bool connected, const std::optional<SessionId>& owner_session_id) {
        std::vector<net::outbound::OutgoingMessage> out_msgs;

        const auto session_ids = ctx.session_manager.getUserSessionIds(user_id);
        for (const auto& session_id : session_ids) {
            auto session_conns = ctx.session_manager.getSessionIdConnections(session_id);
            if (session_conns.empty()) {
                continue;
            }

            sercom::protocol::event::VoiceSelfStatus state;
            state.set_connected(connected);
            state.set_is_owner(connected && owner_session_id.has_value() &&
                               owner_session_id.value() == session_id);

            std::string bytes =
                proto_builders::serialize_envelope(sercom::protocol::Envelope::VOICE_SELF_STATUS,
                                                   state);

            out_msgs.push_back(net::outbound::OutgoingMessage{
                .priority = net::outbound::OutboundPriority::Low,
                .target = net::outbound::Target::many(std::move(session_conns)),
                .action =
                    net::outbound::Action{std::in_place_type<net::outbound::SendPayload>,
                                          net::outbound::SendPayload{
                                              .payload =
                                                  net::outbound::Payload{std::move(bytes), true}}}});
        }

        return out_msgs;
    };

    auto send_self_revoked = [&](const SessionId& old_owner_session_id) {
        const auto old_owner_conns = ctx.session_manager.getSessionIdConnections(old_owner_session_id);
        utils::EventLogger::instance().log(
            utils::EventCategory::VOICE, user_id.value, "voice_self_revoked_targets", 0,
            "old_owner_session_id=" + std::to_string(old_owner_session_id) +
                " target_conn_count=" + std::to_string(old_owner_conns.size()) +
                " target_conn_ids=" + join_connection_ids(old_owner_conns),
            utils::LogDest::BOTH);
        if (old_owner_conns.empty()) {
            return std::vector<net::outbound::OutgoingMessage>{};
        }
        sercom::protocol::event::VoiceSelfRevoked revoked;
        std::string bytes =
            proto_builders::serialize_envelope(sercom::protocol::Envelope::VOICE_SELF_REVOKED,
                                               revoked);
        return single_outgoing(net::outbound::OutgoingMessage{
            .priority = net::outbound::OutboundPriority::Low,
            .target = net::outbound::Target::many(old_owner_conns),
            .action =
                net::outbound::Action{std::in_place_type<net::outbound::SendPayload>,
                                      net::outbound::SendPayload{.payload = net::outbound::Payload{
                                                                     std::move(bytes), true}}}});
    };

    auto push_takeover_required_error = [&]() {
        return make_command_error(
            event->conn_id, env.type(),
            sercom::protocol::event::CommandErrorCode_VOICE_TAKEOVER_REQUIRED,
            "Voice takeover required");
    };

    auto resolve_channel_hub = [&](const ChannelId& channel) -> std::optional<HubId> {
        auto resolved_channel = ctx.channel_service.getChannel(channel);
        if (!resolved_channel) {
            return std::nullopt;
        }
        return resolved_channel->hub_id;
    };

    auto build_token_message =
        [&](const ChannelId& target_channel, bool channel_empty_before_join)
        -> std::expected<net::outbound::OutgoingMessage, net::outbound::OutgoingMessage> {
        services::livekit::LiveKitTokenService::TokenRequest token_req{
            user_id, target_channel, true, true, std::chrono::seconds{3600},
        };

        try {
            const std::string token = ctx.livekit_token_service.mint_token(token_req);
            const std::string e2ee_key = ctx.livekit_token_service.get_or_create_e2ee_key(
                target_channel, channel_empty_before_join);

            sercom::protocol::event::VoiceTokenIssued issued;
            issued.set_channel_id(target_channel.value);
            issued.set_token(token);
            issued.set_expires_in(static_cast<uint64_t>(token_req.ttl.count()));
            issued.set_e2ee_key(e2ee_key);

            std::string bytes = proto_builders::serialize_envelope(
                sercom::protocol::Envelope::VOICE_TOKEN_ISSUED, issued);

            return net::outbound::OutgoingMessage{
                .target = net::outbound::Target::one(event->conn_id),
                .action =
                    net::outbound::Action{std::in_place_type<net::outbound::SendPayload>,
                                          net::outbound::SendPayload{
                                              .payload =
                                                  net::outbound::Payload{std::move(bytes), true}}}};
        } catch (const std::exception& ex) {
            return std::unexpected(make_command_error(
                event->conn_id, env.type(),
                sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR, ex.what()));
        } catch (...) {
            return std::unexpected(make_command_error(
                event->conn_id, env.type(),
                sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
                "Unable to mint voice token"));
        }
    };

    struct VoiceStateSnapshot {
        std::optional<ChannelId> current_channel;
        std::optional<SessionId> current_owner_session_id;
        std::optional<ChannelId> pending_channel;
        std::optional<SessionId> pending_owner_session_id;
    };

    auto voice_state = [&]() -> VoiceStateSnapshot {
        VoiceStateSnapshot snapshot;
        auto current = ctx.session_manager.getSession(user_id);
        if (!current.has_value()) {
            return snapshot;
        }
        snapshot.current_channel = current->current_voice_channel;
        snapshot.current_owner_session_id = current->voice_owner_session;
        snapshot.pending_channel = current->pending_voice_channel;
        snapshot.pending_owner_session_id = current->pending_voice_owner_session;
        return snapshot;
    };

    auto effective_owner_session = [&](const VoiceStateSnapshot& state)
        -> std::optional<SessionId> {
        if (state.pending_owner_session_id.has_value()) {
            return state.pending_owner_session_id;
        }
        return state.current_owner_session_id;
    };

    std::vector<net::outbound::OutgoingMessage> out;

    if (cmd.state() == sercom::protocol::command::VoiceChannelMembership::STATE_REQUEST_LEAVE) {
        const auto state_before = voice_state();
        const bool requester_owns_current =
            state_before.current_channel.has_value() &&
            state_before.current_owner_session_id.has_value() &&
            state_before.current_owner_session_id.value() == requester_session_id;
        const bool requester_owns_pending =
            state_before.pending_channel.has_value() &&
            state_before.pending_owner_session_id.has_value() &&
            state_before.pending_owner_session_id.value() == requester_session_id;

        if (!requester_owns_current && !requester_owns_pending) {
            if (effective_owner_session(state_before).has_value()) {
                return single_outgoing(make_command_error(
                    event->conn_id, env.type(),
                    sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                    "Voice is connected from another session"));
            }
            return out;
        }

        const bool matches_current =
            requester_owns_current && state_before.current_channel.value() == *channel_id_opt;
        const bool matches_pending =
            requester_owns_pending && state_before.pending_channel.value() == *channel_id_opt;
        if (!matches_current && !matches_pending) {
            return single_outgoing(make_command_error(
                event->conn_id, env.type(),
                sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                "Voice leave must target the active or pending voice channel"));
        }

        return out;
    }

    if (cmd.state() == sercom::protocol::command::VoiceChannelMembership::STATE_REQUEST_JOIN) {
        const auto state_before = voice_state();
        const auto owner_session_id = effective_owner_session(state_before);
        if (owner_session_id.has_value() && owner_session_id.value() != requester_session_id) {
            return single_outgoing(push_takeover_required_error());
        }

        const bool target_empty =
            state_before.current_channel.has_value() &&
                    state_before.current_channel.value() == *channel_id_opt
                ? false
                : ctx.session_manager.voiceParticipantsInChannel(*channel_id_opt).empty();
        auto token_msg = build_token_message(*channel_id_opt, target_empty);
        if (!token_msg.has_value()) {
            return single_outgoing(std::move(token_msg.error()));
        }

        ctx.session_manager.stageVoiceJoin(user_id, requester_session_id, *channel_id_opt);
        out.push_back(std::move(token_msg.value()));
        return out;
    }

    if (cmd.state() == sercom::protocol::command::VoiceChannelMembership::STATE_REQUEST_TAKEOVER) {
        const auto state_before = voice_state();
        const auto owner_session_id = effective_owner_session(state_before);

        const bool target_empty =
            state_before.current_channel.has_value() &&
                    state_before.current_channel.value() == *channel_id_opt
                ? false
                : ctx.session_manager.voiceParticipantsInChannel(*channel_id_opt).empty();
        auto token_msg = build_token_message(*channel_id_opt, target_empty);
        if (!token_msg.has_value()) {
            return single_outgoing(std::move(token_msg.error()));
        }

        if (!owner_session_id.has_value() || owner_session_id.value() == requester_session_id) {
            ctx.session_manager.stageVoiceJoin(user_id, requester_session_id, *channel_id_opt);
            out.push_back(std::move(token_msg.value()));
            return out;
        }

        ctx.session_manager.stageVoiceJoin(user_id, requester_session_id, *channel_id_opt);

        const auto old_owner_conns =
            ctx.session_manager.getSessionIdConnections(owner_session_id.value());
        utils::EventLogger::instance().log(
            utils::EventCategory::VOICE, user_id.value, "voice_takeover", 0,
            "old_owner_session_id=" + std::to_string(owner_session_id.value()) +
                " new_owner_session_id=" + std::to_string(requester_session_id) +
                " old_owner_conn_count=" + std::to_string(old_owner_conns.size()) +
                " old_owner_conn_ids=" + join_connection_ids(old_owner_conns) +
                " new_owner_conn_id=" + format_connection_id(event->conn_id),
            utils::LogDest::BOTH);

        auto revoked = send_self_revoked(owner_session_id.value());
        out.insert(out.end(), revoked.begin(), revoked.end());

        if (state_before.current_channel.has_value()) {
            auto status_updates = publish_self_status(true, requester_session_id);
            out.insert(out.end(), status_updates.begin(), status_updates.end());
        }

        out.push_back(std::move(token_msg.value()));
        return out;
    }

    if (cmd.state() == sercom::protocol::command::VoiceChannelMembership::STATE_JOIN) {
        const auto state_before = voice_state();
        const bool requester_has_pending =
            state_before.pending_channel.has_value() &&
            state_before.pending_owner_session_id.has_value() &&
            state_before.pending_owner_session_id.value() == requester_session_id;
        if (!requester_has_pending) {
            const bool already_joined =
                state_before.current_channel.has_value() &&
                state_before.current_owner_session_id.has_value() &&
                state_before.current_owner_session_id.value() == requester_session_id &&
                state_before.current_channel.value() == *channel_id_opt;
            if (already_joined) {
                return out;
            }

            return single_outgoing(make_command_error(
                event->conn_id, env.type(),
                sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                "No pending voice join for this session"));
        }

        if (state_before.pending_channel.value() != *channel_id_opt) {
            return single_outgoing(make_command_error(
                event->conn_id, env.type(),
                sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                "Voice join must target the pending voice channel"));
        }

        const auto previous_channel = state_before.current_channel;
        if (!ctx.session_manager.commitStagedVoiceJoin(user_id, requester_session_id,
                                                       *channel_id_opt)) {
            return single_outgoing(make_command_error(
                event->conn_id, env.type(),
                sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                "Unable to commit voice join"));
        }

        if (!previous_channel.has_value()) {
            utils::EventLogger::instance().voice_join(user_id.value, channel_id_opt->value);
            auto updates = publish_participants(hub_id, *channel_id_opt);
            out.insert(out.end(), updates.begin(), updates.end());
            auto presence_updates = publish_presence(hub_id, *channel_id_opt,
                                                     sercom::protocol::event::ACTIVITY_JOINED);
            out.insert(out.end(), presence_updates.begin(), presence_updates.end());
        } else if (previous_channel.value() != *channel_id_opt) {
            utils::EventLogger::instance().voice_leave(user_id.value, previous_channel->value);
            const auto old_remaining =
                ctx.session_manager.voiceParticipantsInChannel(*previous_channel);
            if (old_remaining.empty()) {
                ctx.livekit_token_service.clear_e2ee_key(*previous_channel);
            }

            if (auto old_hub = resolve_channel_hub(*previous_channel)) {
                auto old_updates = publish_participants(*old_hub, *previous_channel);
                out.insert(out.end(), old_updates.begin(), old_updates.end());
                auto old_presence = publish_presence(*old_hub, *previous_channel,
                                                    sercom::protocol::event::ACTIVITY_LEFT);
                out.insert(out.end(), old_presence.begin(), old_presence.end());
            }

            utils::EventLogger::instance().voice_join(user_id.value, channel_id_opt->value);
            auto new_updates = publish_participants(hub_id, *channel_id_opt);
            out.insert(out.end(), new_updates.begin(), new_updates.end());
            auto new_presence = publish_presence(hub_id, *channel_id_opt,
                                                 sercom::protocol::event::ACTIVITY_JOINED);
            out.insert(out.end(), new_presence.begin(), new_presence.end());
        }

        auto status_updates = publish_self_status(true, requester_session_id);
        out.insert(out.end(), status_updates.begin(), status_updates.end());
        return out;
    }

    if (cmd.state() == sercom::protocol::command::VoiceChannelMembership::STATE_LEAVE) {
        const auto state_before = voice_state();
        const bool requester_owns_current =
            state_before.current_channel.has_value() &&
            state_before.current_owner_session_id.has_value() &&
            state_before.current_owner_session_id.value() == requester_session_id;
        const bool requester_owns_pending =
            state_before.pending_channel.has_value() &&
            state_before.pending_owner_session_id.has_value() &&
            state_before.pending_owner_session_id.value() == requester_session_id;

        if (!requester_owns_current && !requester_owns_pending) {
            if (effective_owner_session(state_before).has_value()) {
                return single_outgoing(make_command_error(
                    event->conn_id, env.type(),
                    sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                    "Voice is connected from another session"));
            }
            return out;
        }

        const bool matches_current =
            requester_owns_current && state_before.current_channel.value() == *channel_id_opt;
        const bool matches_pending =
            requester_owns_pending && state_before.pending_channel.value() == *channel_id_opt;
        if (!matches_current && !matches_pending) {
            return single_outgoing(make_command_error(
                event->conn_id, env.type(),
                sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                "Voice leave must target the active or pending voice channel"));
        }

        if (!requester_owns_current) {
            ctx.session_manager.clearStagedVoiceJoinIfOwnedBy(user_id, requester_session_id);
            return out;
        }

        const ChannelId leaving_channel = state_before.current_channel.value();
        ctx.session_manager.leaveVoiceChannelIfOwnedBy(user_id, requester_session_id);
        utils::EventLogger::instance().voice_leave(user_id.value, leaving_channel.value);

        const auto remaining = ctx.session_manager.voiceParticipantsInChannel(leaving_channel);
        if (remaining.empty()) {
            ctx.livekit_token_service.clear_e2ee_key(leaving_channel);
        }

        if (auto leaving_hub = resolve_channel_hub(leaving_channel)) {
            auto updates = publish_participants(*leaving_hub, leaving_channel);
            out.insert(out.end(), updates.begin(), updates.end());
            auto presence_updates = publish_presence(*leaving_hub, leaving_channel,
                                                     sercom::protocol::event::ACTIVITY_LEFT);
            out.insert(out.end(), presence_updates.begin(), presence_updates.end());
        }
        auto status_updates = publish_self_status(false, std::nullopt);
        out.insert(out.end(), status_updates.begin(), status_updates.end());
        return out;
    }

    return single_outgoing(make_command_error(
        event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
        "Unsupported voice membership state"));
}

}  // namespace app
