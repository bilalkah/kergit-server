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

#include <chrono>
#include <optional>
#include <vector>

namespace app {

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

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!user_exp.has_value()) {
        return {make_drop_connection(event->conn_id,
                                     sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
                                     "Authenticate first")};
    }
    const UserId user_id = user_exp.value();

    auto hub_id_opt = ctx.ids.to_internal(PublicHubId{cmd.hub_id()});
    if (!hub_id_opt.has_value()) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_NOT_FOUND,
            "Hub not found"));
    }

    auto channel_id_opt = ctx.ids.to_internal(PublicChannelId{cmd.channel_id()});
    if (!channel_id_opt.has_value()) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_NOT_FOUND,
            "Channel not found"));
    }

    auto channel_opt = ctx.channel_service.getChannel(*channel_id_opt);
    if (!channel_opt || channel_opt->hub_id != *hub_id_opt) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_NOT_FOUND,
            "Channel not found"));
    }

    if (channel_opt->type != ChannelType::VOICE) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "Channel is not a voice channel"));
    }

    if (!ctx.hub_service.isHubMember(*hub_id_opt, user_id)) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_FORBIDDEN,
            "Join the hub before joining voice"));
    }

    const auto session = ctx.session_manager.getSession(user_id);
    std::optional<HubId> prev_voice_hub;
    std::optional<ChannelId> prev_voice_channel;
    if (session && session->current_voice_hub && session->current_voice_channel) {
        prev_voice_hub = session->current_voice_hub;
        prev_voice_channel = session->current_voice_channel;
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
        participants.set_hub_id(ctx.ids.to_public(hub).value);
        participants.set_channel_id(ctx.ids.to_public(channel).value);

        const auto users = ctx.session_manager.voiceParticipantsInChannel(hub, channel);
        for (const auto& uid : users) {
            participants.add_participant_user_ids(ctx.ids.to_public(uid).value);
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
                                sercom::protocol::event::VoiceChannelPresence_State state) {
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

        auto presence = proto_builders::voice::make_voice_presence(
            ctx.ids.to_public(hub).value, ctx.ids.to_public(channel).value,
            ctx.ids.to_public(user_id).value, state);
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

    std::vector<net::outbound::OutgoingMessage> out;

    if (cmd.state() == sercom::protocol::command::VoiceChannelMembership::STATE_REQUEST_LEAVE) {
        return out;
    }

    if (cmd.state() == sercom::protocol::command::VoiceChannelMembership::STATE_REQUEST_JOIN) {
        // Check if voice channel currently has participants
        const auto participants =
            ctx.session_manager.voiceParticipantsInChannel(*hub_id_opt, *channel_id_opt);
        const bool is_channel_empty = participants.empty();

        services::livekit::LiveKitTokenService::TokenRequest token_req{
            user_id, *channel_id_opt, true, true, std::chrono::seconds{3600},
        };

        std::string token;
        std::string e2ee_key;
        try {
            token = ctx.livekit_token_service.mint_token(token_req);
            // Get or create E2EE key - generates new unique key if channel is empty
            e2ee_key =
                ctx.livekit_token_service.get_or_create_e2ee_key(*channel_id_opt, is_channel_empty);
        } catch (const std::exception& ex) {
            return single_outgoing(make_command_error(
                event->conn_id, env.type(),
                sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR, ex.what()));
        } catch (...) {
            return single_outgoing(
                make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
                                   "Unable to mint voice token"));
        }

        sercom::protocol::event::VoiceTokenIssued issued;
        issued.set_channel_id(ctx.ids.to_public(*channel_id_opt).value);
        issued.set_token(token);
        issued.set_expires_in(static_cast<uint64_t>(token_req.ttl.count()));
        issued.set_e2ee_key(e2ee_key);

        std::string bytes = proto_builders::serialize_envelope(
            sercom::protocol::Envelope::VOICE_TOKEN_ISSUED, issued);

        out.push_back(net::outbound::OutgoingMessage{
            .target = net::outbound::Target::one(event->conn_id),
            .action =
                net::outbound::Action{std::in_place_type<net::outbound::SendPayload>,
                                      net::outbound::SendPayload{.payload = net::outbound::Payload{
                                                                     std::move(bytes), true}}}});

        return out;
    }

    if (cmd.state() == sercom::protocol::command::VoiceChannelMembership::STATE_LEAVE) {
        if (prev_voice_hub && prev_voice_channel && *prev_voice_hub == *hub_id_opt &&
            *prev_voice_channel == *channel_id_opt) {
            ctx.session_manager.leaveVoiceChannel(user_id);

            // Check if channel is now empty, if so clear the E2EE key
            const auto remaining =
                ctx.session_manager.voiceParticipantsInChannel(*hub_id_opt, *channel_id_opt);
            if (remaining.empty()) {
                ctx.livekit_token_service.clear_e2ee_key(*channel_id_opt);
            }

            auto updates = publish_participants(*hub_id_opt, *channel_id_opt);
            out.insert(out.end(), updates.begin(), updates.end());
            auto presence_updates =
                publish_presence(*hub_id_opt, *channel_id_opt,
                                 sercom::protocol::event::VoiceChannelPresence::STATE_LEFT);
            out.insert(out.end(), presence_updates.begin(), presence_updates.end());
        }
        return out;
    }

    if (cmd.state() == sercom::protocol::command::VoiceChannelMembership::STATE_JOIN) {
        if (prev_voice_hub && prev_voice_channel && *prev_voice_hub == *hub_id_opt &&
            *prev_voice_channel == *channel_id_opt) {
            return {};
        }
        if (prev_voice_hub && prev_voice_channel &&
            (*prev_voice_hub != *hub_id_opt || *prev_voice_channel != *channel_id_opt)) {
            ctx.session_manager.leaveVoiceChannel(user_id);

            // Check if previous channel is now empty, if so clear the E2EE key
            const auto remaining = ctx.session_manager.voiceParticipantsInChannel(
                *prev_voice_hub, *prev_voice_channel);
            if (remaining.empty()) {
                ctx.livekit_token_service.clear_e2ee_key(*prev_voice_channel);
            }

            auto updates = publish_participants(*prev_voice_hub, *prev_voice_channel);
            out.insert(out.end(), updates.begin(), updates.end());
            auto presence_updates =
                publish_presence(*prev_voice_hub, *prev_voice_channel,
                                 sercom::protocol::event::VoiceChannelPresence::STATE_LEFT);
            out.insert(out.end(), presence_updates.begin(), presence_updates.end());
        }

        ctx.session_manager.joinVoiceChannel(user_id, *hub_id_opt, *channel_id_opt);
        auto updates = publish_participants(*hub_id_opt, *channel_id_opt);
        out.insert(out.end(), updates.begin(), updates.end());
        auto presence_updates =
            publish_presence(*hub_id_opt, *channel_id_opt,
                             sercom::protocol::event::VoiceChannelPresence::STATE_JOINED);
        out.insert(out.end(), presence_updates.begin(), presence_updates.end());
        return out;
    }

    return single_outgoing(make_command_error(
        event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
        "Voice channel state is unspecified"));
}

}  // namespace app
