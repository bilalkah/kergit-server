#include "app/commands/activity/JoinVoiceChannelCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "domains/Channel.h"
#include "proto/command/activity.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/activity.pb.h"
#include "proto/event/error.pb.h"
#include "utils/EventLogger.h"

#include <cassert>
#include <cstdint>
#include <exception>
#include <string>
#include <vector>

namespace app {
namespace {

std::string make_voice_self_status_disconnected() {
    sercom::protocol::event::VoiceSelfStatus status;
    status.set_connected(false);
    status.set_is_owner(false);

    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(sercom::protocol::Envelope::VOICE_SELF_STATUS);
    status.SerializeToString(env.mutable_payload());

    std::string bytes;
    env.SerializeToString(&bytes);
    return bytes;
}

std::string make_voice_self_revoked() {
    sercom::protocol::event::VoiceSelfRevoked revoked;

    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(sercom::protocol::Envelope::VOICE_SELF_REVOKED);
    revoked.SerializeToString(env.mutable_payload());

    std::string bytes;
    env.SerializeToString(&bytes);
    return bytes;
}

std::string make_voice_token_issued(const std::string& token, const std::string& livekit_url,
                                    uint64_t expires_in, const std::string& e2ee_key) {
    sercom::protocol::event::VoiceTokenIssued issued;
    issued.set_token(token);
    issued.set_livekit_url(livekit_url);
    issued.set_expires_in(expires_in);
    issued.set_e2ee_key(e2ee_key);

    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(sercom::protocol::Envelope::VOICE_TOKEN_ISSUED);
    issued.SerializeToString(env.mutable_payload());

    std::string bytes;
    env.SerializeToString(&bytes);
    return bytes;
}

}  // namespace

std::vector<net::outbound::OutgoingMessage> JoinVoiceChannelCommand::execute(
    CommandContext& ctx, const queue::Event& evt) {
    std::vector<net::outbound::OutgoingMessage> out;
    assert(std::holds_alternative<queue::MessageEvent>(evt));
    const auto* event = &std::get<queue::MessageEvent>(evt);
    const auto& env = event->payload.env;
    assert(env.type() == sercom::protocol::Envelope::VOICE_JOIN);

    const auto& cmd = require_parsed<sercom::protocol::command::JoinVoiceChannelRequest>(*event);
    const ChannelId channel_id{cmd.channel_id()};
    const HubId hub_id{cmd.hub_id()};

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);

    utils::EventLogger::instance().log(
        utils::EventCategory::VOICE, user_exp.has_value() ? user_exp->value : "",
        "JoinVoiceChannelRequest", 0,
        "conn:" + event->conn_id.netstack_id.value + ":" + event->conn_id.conn_id.value +
            " channel:" + (channel_id.value.empty() ? "unknown" : channel_id.value),
        utils::LogDest::FILE);

    if (!user_exp.has_value()) {
        out.emplace_back(make_drop_connection(
            event->conn_id, sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
            "Authenticate first"));
        return out;
    }
    const UserId user_id = user_exp.value();

    auto requester_session_id_exp = ctx.session_manager.sessionIdOfConnection(event->conn_id);
    if (!requester_session_id_exp.has_value()) {
        out.emplace_back(make_drop_connection(
            event->conn_id, sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
            "Runtime session missing"));
        return out;
    }
    const SessionId requester_session_id = requester_session_id_exp.value();

    auto channel_opt = ctx.channel_service.getChannel(channel_id);
    if (!channel_opt) {
        out.emplace_back(make_command_error(event->conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_NOT_FOUND,
                                            "Channel not found"));
        return out;
    }

    if (channel_opt->hub_id != hub_id) {
        out.emplace_back(make_command_error(event->conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_NOT_FOUND,
                                            "Channel not found"));
        return out;
    }

    if (channel_opt->type != ChannelType::VOICE) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "Channel is not a voice channel"));
        return out;
    }

    if (!ctx.hub_service.isHubMember(hub_id, user_id)) {
        out.emplace_back(make_command_error(event->conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                            "Join the hub before joining voice"));
        return out;
    }

    auto& voice_sessions = ctx.voice_service.sessions();
    const auto old_owner_session = voice_sessions.user_session(user_id);
    const auto old_channel = voice_sessions.user_channel(user_id);

    const bool taking_over =
        old_owner_session.has_value() && *old_owner_session != requester_session_id;
    const bool switching_channel = old_owner_session.has_value() &&
                                   *old_owner_session == requester_session_id &&
                                   old_channel.has_value() && *old_channel != channel_id;

    if ((taking_over || switching_channel) && old_channel.has_value()) {
        if (taking_over && *old_channel == channel_id &&
            voice_sessions.participants_in_channel(*old_channel).size() == 1) {
            ctx.voice_service.mark_channel_takeover(*old_channel);
        }

        if (!ctx.voice_service.verified_kick_user(*old_channel, user_id)) {
            out.emplace_back(make_command_error(
                event->conn_id, env.type(),
                sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
                "Failed to revoke previous voice session"));
            return out;
        }

        if (taking_over && old_owner_session.has_value()) {
            auto old_conns = ctx.session_manager.getSessionIdConnections(*old_owner_session);
            if (!old_conns.empty()) {
                // Revoke is session-scoped: only notify the session that previously owned voice.
                auto revoked_targets = old_conns;
                auto revoked_msg =
                    make_outgoing_message(net::outbound::Target::many(std::move(revoked_targets)),
                                                         make_voice_self_revoked());
                out.push_back(std::move(revoked_msg));

                auto disconnected_msg =
                    make_outgoing_message(net::outbound::Target::many(std::move(old_conns)),
                                          make_voice_self_status_disconnected());
                out.push_back(std::move(disconnected_msg));
            }
        }
    }

    bool pending_muted = cmd.prefer_muted();
    bool pending_deafened = cmd.prefer_deafened();
    if (pending_deafened) {
        pending_muted = true;
    }

    // Correlates command-path join intent with webhook events for stale/race rejection.
    const std::string intent_nonce = ctx.voice_service.generate_intent_nonce();

    services::voice::VoiceService::JoinVoiceToken issued;
    try {
        issued =
            ctx.voice_service.join_voice(channel_id, user_id, requester_session_id, intent_nonce);
    } catch (const std::exception& ex) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(),
            sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR, ex.what()));
        return out;
    }

    if (issued.token.empty() || issued.livekit_url.empty()) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(),
            sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
            "Voice token issuance failed"));
        return out;
    }

    services::voice::VoiceService::PendingJoinIntent pending;
    pending.session_id = requester_session_id;
    pending.intent_nonce = intent_nonce;
    pending.to_channel = channel_id;
    pending.muted = pending_muted;
    pending.deafened = pending_deafened;

    if (old_channel.has_value() && *old_channel != channel_id) {
        pending.has_from_channel = true;
        pending.from_channel = *old_channel;
    }

    if (!ctx.voice_service.stage_pending_join_intent(user_id, pending, issued.expires_in)) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(),
            sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
            "Failed to stage voice join intent"));
        return out;
    }

    utils::EventLogger::instance().log(
        utils::EventCategory::VOICE, user_id.value, "voice_token_issued", 0,
        "session=" + std::to_string(requester_session_id) + " channel=" + channel_id.value +
            " token_len=" + std::to_string(issued.token.size()) +
            " livekit_url=" + issued.livekit_url,
        utils::LogDest::FILE);

    std::string token_bytes = make_voice_token_issued(
        issued.token, issued.livekit_url, issued.expires_in, issued.e2ee_key);
    out.push_back(
        make_outgoing_message(net::outbound::Target::one(event->conn_id), std::move(token_bytes)));

    return out;
}

}  // namespace app
