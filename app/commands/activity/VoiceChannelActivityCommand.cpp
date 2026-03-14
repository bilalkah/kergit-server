#include "app/commands/activity/VoiceChannelActivityCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "domains/Channel.h"
#include "proto/command/activity.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/activity.pb.h"
#include "proto/event/error.pb.h"

#include <cassert>
#include <string_view>
#include <vector>

namespace app {
namespace {

std::string make_voice_state(std::string_view user_id, bool muted, bool deafened) {
    sercom::protocol::event::VoiceState voice_state;
    voice_state.mutable_participant()->set_user_id(user_id.data(), user_id.size());
    voice_state.mutable_participant()->set_muted(muted);
    voice_state.mutable_participant()->set_deafened(deafened);

    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(sercom::protocol::Envelope::VOICE_STATE_EVENT);
    voice_state.SerializeToString(env.mutable_payload());

    std::string bytes;
    env.SerializeToString(&bytes);
    return bytes;
}

std::string make_voice_self_status_connected(bool is_owner, const ChannelId& channel) {
    sercom::protocol::event::VoiceSelfStatus self_status;
    self_status.set_connected(true);
    self_status.set_is_owner(is_owner);
    self_status.set_channel_id(channel.value);

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

    const ChannelId channel_id{cmd.channel_id()};
    const HubId hub_id{cmd.hub_id()};

    auto channel_opt = ctx.channel_service.getChannel(channel_id);
    if (!channel_opt) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_NOT_FOUND,
            "Channel not found"));
        return out;
    }

    if (channel_opt->hub_id != hub_id) {
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

    // Command carries the full desired voice state. Deafening always implies muted.
    const bool desired_deafened = cmd.is_deafened();
    const bool desired_muted = cmd.is_muted() || desired_deafened;

    if (current_muted == desired_muted && current_deafened == desired_deafened) {
        return {};
    }

    bool changed = false;
    if (current_deafened != desired_deafened) {
        changed = voice_sessions.set_deafened(user_id, desired_deafened) || changed;
    }

    // set_deafened() also mutates mute, so re-read before applying desired mute.
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

    // Hub-wide mute/deafen delta.
    utils::metrics::counters().fanout_subscriber_snapshot_total.fetch_add(1,
                                                                           std::memory_order_relaxed);
    auto subs = ctx.subscription_manager.getSubscribers(Topic::HubTopic(hub_id));
    if (subs && !subs->empty()) {
        std::vector<GlobalConnId> conns{subs->begin(), subs->end()};
        auto voice_state_msg =
            make_outgoing_message(net::outbound::Target::many(std::move(conns)),
                                  make_voice_state(user_id.value, final_muted, final_deafened));
        out.emplace_back(std::move(voice_state_msg));
    }

    // Voice ownership/connection state for all of user's sessions.
    for (const auto& session_id : ctx.session_manager.getUserSessionIds(user_id)) {
        auto session_conns = ctx.session_manager.getSessionIdConnections(session_id);
        if (session_conns.empty()) continue;

        auto self_status_msg = make_outgoing_message(
            net::outbound::Target::many(std::move(session_conns)),
            make_voice_self_status_connected(owner_session.has_value() &&
                                                 *owner_session == session_id,
                                             channel_id));
        out.emplace_back(std::move(self_status_msg));
    }

    return out;
}

}  // namespace app
