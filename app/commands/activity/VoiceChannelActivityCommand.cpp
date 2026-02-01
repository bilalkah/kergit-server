#include "app/commands/activity/VoiceChannelActivityCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "domains/Channel.h"
#include "proto/command/activity.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/activity.pb.h"
#include "proto/event/error.pb.h"

#include <optional>
#include <vector>

namespace app {

std::vector<net::outbound::OutgoingMessage> VoiceChannelActivityCommand::execute(
    CommandContext& ctx, const queue::Event& evt) {
    const auto* event = std::get_if<queue::MessageEvent>(&evt);
    if (!event) {
        return {};
    }

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

    auto hub_id_opt = ctx.ids.to_internal(PublicHubId{cmd.hub_id()});
    if (!hub_id_opt.has_value()) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_NOT_FOUND,
                                   "Hub not found"));
    }

    auto channel_id_opt = ctx.ids.to_internal(PublicChannelId{cmd.channel_id()});
    if (!channel_id_opt.has_value()) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_NOT_FOUND,
                                   "Channel not found"));
    }

    auto channel_opt = ctx.channel_service.getChannel(*channel_id_opt);
    if (!channel_opt || channel_opt->hub_id != *hub_id_opt) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_NOT_FOUND,
                                   "Channel not found"));
    }

    if (channel_opt->type != ChannelType::VOICE) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                                   "Channel is not a voice channel"));
    }

    if (!ctx.hub_service.isHubMember(*hub_id_opt, user_id)) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                   "Join the hub before updating voice state"));
    }

    const auto session = ctx.session_manager.getSession(user_id);
    if (!session || !session->current_voice_hub ||
        !session->current_voice_channel) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                                   "Join a voice channel first"));
    }

    if (session->current_voice_hub != *hub_id_opt ||
        session->current_voice_channel != *channel_id_opt) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                                   "Voice activity must target the active voice channel"));
    }

    bool changed = false;
    auto state = cmd.state();
    if (state == sercom::protocol::command::VoiceChannelActivity::STATE_MUTE) {
        changed = ctx.session_manager.setVoiceMuted(user_id, true);
    } else if (state == sercom::protocol::command::VoiceChannelActivity::STATE_UNMUTE) {
        changed = ctx.session_manager.setVoiceMuted(user_id, false);
    } else if (state == sercom::protocol::command::VoiceChannelActivity::STATE_DEAFEN) {
        changed = ctx.session_manager.setVoiceDeafened(user_id, true);
    } else if (state == sercom::protocol::command::VoiceChannelActivity::STATE_UNDEAFEN) {
        changed = ctx.session_manager.setVoiceDeafened(user_id, false);
    } else {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                                   "Voice activity state is unspecified"));
    }

    if (!changed) {
        return {};
    }

    utils::metrics::counters().fanout_subscriber_snapshot_total.fetch_add(
        1, std::memory_order_relaxed);
    auto subs = ctx.subscription_manager.getSubscribers(Topic::HubTopic(*hub_id_opt));
    if (!subs || subs->empty()) {
        return {};
    }

    std::vector<GlobalConnId> conns;
    conns.reserve(subs->size());
    for (const auto& conn : *subs) {
        conns.push_back(conn);
    }
    if (conns.empty()) {
        return {};
    }

    sercom::protocol::event::VoiceChannelPresence_State presence_state =
        sercom::protocol::event::VoiceChannelPresence::STATE_UNSPECIFIED;
    if (state == sercom::protocol::command::VoiceChannelActivity::STATE_MUTE) {
        presence_state = sercom::protocol::event::VoiceChannelPresence::STATE_MUTED;
    } else if (state == sercom::protocol::command::VoiceChannelActivity::STATE_UNMUTE) {
        presence_state = sercom::protocol::event::VoiceChannelPresence::STATE_UNMUTED;
    } else if (state == sercom::protocol::command::VoiceChannelActivity::STATE_DEAFEN) {
        presence_state = sercom::protocol::event::VoiceChannelPresence::STATE_DEAFENED;
    } else if (state == sercom::protocol::command::VoiceChannelActivity::STATE_UNDEAFEN) {
        presence_state = sercom::protocol::event::VoiceChannelPresence::STATE_UNDEAFENED;
    }

    sercom::protocol::event::VoiceChannelPresence presence;
    presence.set_hub_id(ctx.ids.to_public(*hub_id_opt).value);
    presence.set_channel_id(ctx.ids.to_public(*channel_id_opt).value);
    presence.set_state(presence_state);
    presence.set_user_id(ctx.ids.to_public(user_id).value);

    sercom::protocol::Envelope out_env;
    out_env.set_version(1);
    out_env.set_type(sercom::protocol::Envelope::VOICE_CHANNEL_PRESENCE);
    presence.SerializeToString(out_env.mutable_payload());

    std::string bytes;
    out_env.SerializeToString(&bytes);

    return single_outgoing(net::outbound::OutgoingMessage{
        .priority = net::outbound::OutboundPriority::Low,
        .target = net::outbound::Target::many(std::move(conns)),
        .action =
            net::outbound::Action{std::in_place_type<net::outbound::SendPayload>,
                                  net::outbound::SendPayload{.payload = net::outbound::Payload{std::move(bytes), true}}}});
}

}  // namespace app
