#include "app/commands/activity/VoiceChannelActivityCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "domains/Channel.h"
#include "proto/command/activity.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/activity.pb.h"
#include "proto/event/error.pb.h"
#include "proto/event/state.pb.h"

#include <cassert>
#include <optional>
#include <vector>

namespace app {
namespace {

std::string make_voice_self_status_connected(bool is_owner, const HubId& hub_id,
                                             const ChannelId& channel_id,
                                             const std::optional<std::string>& resume_id) {
    sercom::protocol::event::VoiceSelfStatus self_status;
    self_status.set_connected(true);
    self_status.set_is_owner(is_owner);
    *self_status.mutable_channel() = to_proto_channel_ref(hub_id, channel_id);
    if (is_owner && resume_id.has_value() && !resume_id->empty()) {
        self_status.set_resume_id(*resume_id);
    }

    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(sercom::protocol::Envelope::VOICE_SELF_STATUS);
    self_status.SerializeToString(env.mutable_payload());

    std::string bytes;
    env.SerializeToString(&bytes);
    return bytes;
}

}  // namespace

std::vector<net::outbound::OutgoingMessage> VoiceChannelActivityCommand::execute(
    CommandContext& ctx, const queue::Event& evt) {
    std::vector<net::outbound::OutgoingMessage> out;
    assert(std::holds_alternative<queue::MessageEvent>(evt));
    const auto* event = &std::get<queue::MessageEvent>(evt);
    const auto& env = event->payload.env;
    assert(env.type() == sercom::protocol::Envelope::VOICE_ACTIVITY);

    const auto& cmd = require_parsed<sercom::protocol::command::VoiceChannelActivity>(*event);
    const auto scope_opt = to_channel_scope(cmd.channel());
    if (!scope_opt.has_value()) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "channel.hub_id and channel.channel_id are required"));
        return out;
    }
    const HubId hub_id = scope_opt->hub_id;
    const ChannelId channel_id = scope_opt->channel_id;

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!user_exp.has_value()) {
        out.emplace_back(make_drop_connection(event->conn_id,
                                              sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
                                              "Authenticate first"));
        return out;
    }
    const UserId user_id = user_exp.value();

    auto requester_session_id_exp = ctx.session_manager.sessionIdOfConnection(event->conn_id);
    if (!requester_session_id_exp.has_value()) {
        out.emplace_back(make_drop_connection(event->conn_id,
                                              sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
                                              "Runtime session missing"));
        return out;
    }
    const SessionId requester_session_id = requester_session_id_exp.value();

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

    if (!ctx.hub_service.isHubMember(hub_id, user_id)) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_FORBIDDEN,
            "Join the hub before updating voice state"));
        return out;
    }

    auto& voice_sessions = ctx.voice_service.sessions();
    const auto current_channel = voice_sessions.user_channel(user_id);
    const auto owner_session = voice_sessions.user_session(user_id);

    if (!current_channel.has_value()) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "Join a voice channel first"));
        return out;
    }

    if (owner_session.has_value() && *owner_session != requester_session_id) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_FORBIDDEN,
            "Voice is connected from another session"));
        return out;
    }

    if (*current_channel != channel_id) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "Voice activity must target the active voice channel"));
        return out;
    }

    bool current_muted = false;
    bool current_deafened = false;
    bool has_current_state = false;
    for (const auto& p : voice_sessions.participants_in_channel(channel_id)) {
        if (p.user_id == user_id) {
            current_muted = p.muted;
            current_deafened = p.deafened;
            has_current_state = true;
            break;
        }
    }
    if (!has_current_state) return {};

    const bool desired_deafened = cmd.is_deafened();
    const bool desired_muted = cmd.is_muted() || desired_deafened;

    if (current_muted == desired_muted && current_deafened == desired_deafened) {
        return {};
    }

    bool changed = false;
    if (current_deafened != desired_deafened) {
        changed = voice_sessions.set_deafened(user_id, desired_deafened) || changed;
    }

    current_muted = false;
    current_deafened = false;
    has_current_state = false;
    for (const auto& p : voice_sessions.participants_in_channel(channel_id)) {
        if (p.user_id == user_id) {
            current_muted = p.muted;
            current_deafened = p.deafened;
            has_current_state = true;
            break;
        }
    }
    if (!has_current_state) return {};

    if (current_muted != desired_muted) {
        changed = voice_sessions.set_muted(user_id, desired_muted) || changed;
    }

    if (!changed) return {};

    bool final_muted = false;
    bool final_deafened = false;
    for (const auto& p : voice_sessions.participants_in_channel(channel_id)) {
        if (p.user_id == user_id) {
            final_muted = p.muted;
            final_deafened = p.deafened;
            break;
        }
    }

    utils::metrics::counters().fanout_subscriber_snapshot_total.fetch_add(
        1, std::memory_order_relaxed);
    auto subs = ctx.subscription_manager.getSubscribers(Topic::HubTopic(hub_id));
    if (subs && !subs->empty()) {
        sercom::protocol::event::StateDelta delta;
        auto* hub_delta = delta.add_hubs();
        hub_delta->set_hub_id(hub_id.value);
        auto* channel_delta = hub_delta->add_channels();
        channel_delta->set_channel_id(channel_id.value);
        auto* upsert = channel_delta->add_voice_ops()->mutable_upsert();
        upsert->mutable_participant()->set_user_id(user_id.value);
        upsert->mutable_participant()->set_muted(final_muted);
        upsert->mutable_participant()->set_deafened(final_deafened);

        std::vector<GlobalConnId> conns{subs->begin(), subs->end()};
        out.emplace_back(make_outgoing_message(net::outbound::Target::many(std::move(conns)),
                                               make_state_delta(delta)));
    }

    for (const auto& session_id : ctx.session_manager.getUserSessionIds(user_id)) {
        auto session_conns = ctx.session_manager.getSessionIdConnections(session_id);
        if (session_conns.empty()) continue;

        const bool is_owner_session =
            owner_session.has_value() && *owner_session == session_id;
        out.emplace_back(make_outgoing_message(
            net::outbound::Target::many(std::move(session_conns)),
            make_voice_self_status_connected(is_owner_session, hub_id, channel_id,
                                             is_owner_session
                                                 ? ctx.voice_service.current_resume_id(user_id)
                                                 : std::nullopt)));
    }

    return out;
}

}  // namespace app
