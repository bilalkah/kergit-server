#include "app/commands/system/DisconnectionCommand.h"

#include "app/commands/utils.h"
#include "app/managers/subscription/Topic.h"
#include "app/proto_builders/EnvelopeBuilders.h"
#include "app/proto_builders/VoiceBuilders.h"
#include "proto/envelope.pb.h"
#include "proto/event/activity.pb.h"
#include "utils/EventLogger.h"

#include <string>
#include <unordered_set>
#include <variant>
#include <vector>

namespace app {

namespace {

std::string format_connection_id(const GlobalConnId& conn) {
    return conn.netstack_id.value + ":" + conn.conn_id.value;
}

}  // namespace

std::vector<net::outbound::OutgoingMessage> DisconnectionCommand::execute(CommandContext& ctx,
                                                                          const queue::Event& evt) {
    std::vector<net::outbound::OutgoingMessage> out;
    const auto* event = std::get_if<queue::DisconnectionEvent>(&evt);
    if (!event) {
        utils::EventLogger::instance().log(utils::EventCategory::DEBUG, "", "DISCONNECT_INVALID", 0,
                                           "DisconnectionCommand: invalid event type");
        return out;
    }

    auto session_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!session_exp.has_value()) {
        return out;  // unknown connection, ignore
    }

    auto session_id_exp = ctx.session_manager.sessionIdOfConnection(event->conn_id);
    if (!session_id_exp.has_value()) {
        return out;
    }

    const UserId user_id = session_exp.value();
    const SessionId session_id = session_id_exp.value();

    utils::EventLogger::instance().log(
        utils::EventCategory::SESSION, user_id.value, "connection_closed", 0,
        "conn_id=" + format_connection_id(event->conn_id) +
            " session_id=" + std::to_string(session_id) +
            " close_code=" + std::to_string(event->code) + " reason=" + event->reason);

    // Capture voice channel info before removing the connection.
    std::optional<ChannelId> voice_channel;
    std::optional<SessionId> voice_owner_session_id;
    bool disconnected_owner_session = false;
    {
        auto session_info = ctx.session_manager.getSession(user_id);
        if (session_info.has_value() && session_info->current_voice_channel &&
            session_info->voice_owner_session) {
            voice_channel = session_info->current_voice_channel;
            voice_owner_session_id = session_info->voice_owner_session;
            disconnected_owner_session = session_info->voice_owner_session.value() == session_id;
        }
    }

    const auto hubid_list = [&]() {
        std::vector<HubId> hubid_list;
        const auto conn_subs =
            ctx.subscription_manager.getSubscriptionsForConnection(event->conn_id);
        if (!conn_subs.has_value()) {
            return hubid_list;
        }
        for (const auto& topic : conn_subs.value()) {
            if (topic.kind != TopicKind::Hub) continue;
            hubid_list.emplace_back(topic_utils::extractHubId(topic));
        }
        return hubid_list;
    }();

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

        auto presence = proto_builders::voice::make_voice_presence(
            channel.value, user_id.value, state);
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
        for (const auto& sid : session_ids) {
            auto session_conns = ctx.session_manager.getSessionIdConnections(sid);
            if (session_conns.empty()) {
                continue;
            }

            sercom::protocol::event::VoiceSelfStatus state;
            state.set_connected(connected);
            state.set_is_owner(connected && owner_session_id.has_value() &&
                               owner_session_id.value() == sid);

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

    ctx.session_manager.removeConnection(event->conn_id);
    ctx.subscription_manager.removeAllForConnection(event->conn_id);

    const auto remaining_owner_session_connections =
        voice_owner_session_id.has_value() && disconnected_owner_session
            ? ctx.session_manager.getSessionIdConnections(*voice_owner_session_id)
            : std::vector<GlobalConnId>{};

    const bool owner_session_still_connected = !remaining_owner_session_connections.empty();

    const bool clear_voice_owner =
        disconnected_owner_session && !owner_session_still_connected;

    if (clear_voice_owner && voice_channel) {
        utils::EventLogger::instance().voice_leave(user_id.value, voice_channel->value);

        const auto remaining = ctx.session_manager.voiceParticipantsInChannel(*voice_channel);
        if (remaining.empty()) {
            ctx.livekit_token_service.clear_e2ee_key(*voice_channel);
        }

        if (auto channel = ctx.channel_service.getChannel(*voice_channel)) {
            auto updates = publish_participants(channel->hub_id, *voice_channel);
            out.insert(out.end(), updates.begin(), updates.end());
            auto presence_updates = publish_presence(channel->hub_id, *voice_channel,
                                                     sercom::protocol::event::ACTIVITY_LEFT);
            out.insert(out.end(), presence_updates.begin(), presence_updates.end());
        }
        auto status_updates = publish_self_status(false, std::nullopt);
        out.insert(out.end(), status_updates.begin(), status_updates.end());
    }

    // User is still online through another active connection.
    if (ctx.session_manager.hasSession(user_id)) {
        return out;
    }

    for (const auto& hub_id : hubid_list) {
        const auto online_members = ctx.presence_manager.onlineUsersInHub(hub_id);
        std::unordered_set<GlobalConnId> recipient_set;

        for (const auto& member_id : online_members) {
            if (member_id == user_id) continue;
            const auto member_conns = ctx.session_manager.getSessionConnections(member_id);
            for (const auto& conn : member_conns) {
                recipient_set.insert(conn);
            }
        }

        std::vector<GlobalConnId> recipients;
        recipients.reserve(recipient_set.size());
        for (const auto& conn : recipient_set) {
            recipients.push_back(conn);
        }

        if (!recipients.empty()) {
            auto payload = ctx.hub_notifier.memberOffline(hub_id, user_id);
            out.push_back(net::outbound::OutgoingMessage{
                .priority = net::outbound::OutboundPriority::Low,
                .target = net::outbound::Target::many(std::move(recipients)),
                .action = net::outbound::Action{
                    std::in_place_type<net::outbound::SendPayload>,
                    net::outbound::SendPayload{
                        .payload = net::outbound::Payload{std::move(payload), true}}}});
        }
    }

    return out;
}

}  // namespace app
