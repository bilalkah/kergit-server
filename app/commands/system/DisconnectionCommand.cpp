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

net::outbound::OutgoingMessage make_broadcast(std::vector<GlobalConnId> conns, std::string bytes) {
    return net::outbound::OutgoingMessage{
        .priority = net::outbound::OutboundPriority::Low,
        .target = net::outbound::Target::many(std::move(conns)),
        .action = net::outbound::Action{
            std::in_place_type<net::outbound::SendPayload>,
            net::outbound::SendPayload{.payload = net::outbound::Payload{std::move(bytes), true}}}};
}

std::vector<GlobalConnId> hub_subscriber_conns(CommandContext& ctx, const HubId& hub) {
    utils::metrics::counters().fanout_subscriber_snapshot_total.fetch_add(
        1, std::memory_order_relaxed);
    auto subs = ctx.subscription_manager.getSubscribers(Topic::HubTopic(hub));
    if (!subs || subs->empty()) return {};
    return {subs->begin(), subs->end()};
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
        return out;
    }

    auto session_id_exp = ctx.session_manager.sessionIdOfConnection(event->conn_id);
    if (!session_id_exp.has_value()) {
        return out;
    }

    const UserId user_id = session_exp.value();
    const SessionId session_id = session_id_exp.value();

    utils::EventLogger::instance().log(
        utils::EventCategory::SESSION, user_id.value, "connection_closed", 0,
        "session_id=" + std::to_string(session_id) + " close_code=" + std::to_string(event->code) +
            " reason=" + event->reason);

    auto& voice_sessions = ctx.voice_service.sessions();

    // Gather hub subscriptions before removing the connection.
    const auto hubid_list = [&]() {
        std::vector<HubId> list;
        const auto conn_subs =
            ctx.subscription_manager.getSubscriptionsForConnection(event->conn_id);
        if (!conn_subs.has_value()) return list;
        for (const auto& topic : conn_subs.value()) {
            if (topic.kind != TopicKind::Hub) continue;
            list.emplace_back(topic_utils::extractHubId(topic));
        }
        return list;
    }();

    ctx.session_manager.removeConnection(event->conn_id);
    ctx.subscription_manager.removeAllForConnection(event->conn_id);

    // If this session has no remaining connections, clear voice only if it still owns it.
    const auto remaining_conns = ctx.session_manager.getSessionIdConnections(session_id);
    if (remaining_conns.empty()) {
        const auto leave_result = voice_sessions.leave_if_owner(user_id, session_id);
        if (leave_result.removed && leave_result.channel.has_value()) {
            const ChannelId voice_channel = *leave_result.channel;
            utils::EventLogger::instance().voice_leave(user_id.value, voice_channel.value);

            // Persist leave to DB (removes channel membership but preferences
            // are already cached in pending_preferences_ if needed for recovery).
            ctx.voice_service.persist_voice_leave(user_id);

            ctx.voice_service.kick_user(voice_channel, user_id);

            if (leave_result.became_empty) {
                ctx.voice_service.on_channel_empty(voice_channel);
            }

            if (auto ch = ctx.channel_service.getChannel(voice_channel)) {
                auto conns = hub_subscriber_conns(ctx, ch->hub_id);
                if (!conns.empty()) {
                    sercom::protocol::event::VoiceChannelParticipants participants;
                    participants.set_channel_id(voice_channel.value);
                    for (const auto& p : voice_sessions.participants_in_channel(voice_channel)) {
                        auto* out_p = participants.add_participants();
                        out_p->set_user_id(p.user_id.value);
                        out_p->set_muted(p.muted);
                        out_p->set_deafened(p.deafened);
                    }
                    out.push_back(make_broadcast(
                        conns,
                        proto_builders::serialize_envelope(
                            sercom::protocol::Envelope::VOICE_CHANNEL_PARTICIPANTS, participants)));

                    auto presence = proto_builders::voice::make_voice_presence(
                        voice_channel.value, user_id.value, sercom::protocol::event::ACTIVITY_LEFT);
                    out.push_back(make_broadcast(
                        std::move(conns),
                        proto_builders::serialize_envelope(
                            sercom::protocol::Envelope::VOICE_CHANNEL_PRESENCE, presence)));
                }
            }

            for (const auto& sid : ctx.session_manager.getUserSessionIds(user_id)) {
                auto session_conns = ctx.session_manager.getSessionIdConnections(sid);
                if (session_conns.empty()) continue;
                sercom::protocol::event::VoiceSelfStatus status;
                status.set_connected(false);
                status.set_is_owner(false);
                out.push_back(
                    make_broadcast(std::move(session_conns),
                                   proto_builders::serialize_envelope(
                                       sercom::protocol::Envelope::VOICE_SELF_STATUS, status)));
            }
        }
    }

    // If user has no remaining connections at all, send offline notifications.
    if (!ctx.session_manager.hasSession(user_id)) {
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

            std::vector<GlobalConnId> recipients(recipient_set.begin(), recipient_set.end());
            if (!recipients.empty()) {
                auto payload = ctx.hub_notifier.memberOffline(hub_id, user_id);
                out.push_back(make_broadcast(std::move(recipients), std::move(payload)));
            }
        }
    }

    return out;
}

}  // namespace app
