#include "app/commands/session/BootstrapCommand.h"

#include "app/commands/session/StateSyncBuilder.h"
#include "app/commands/utils.h"
#include "app/managers/subscription/Topic.h"
#include "proto/envelope.pb.h"
#include "proto/event/activity.pb.h"

#include <unordered_set>
#include <utility>
#include <vector>

namespace app {
namespace {

template <typename TPayload>
std::string serialize_as_envelope(const sercom::protocol::Envelope::Type type,
                                  const TPayload& payload) {
    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(type);
    payload.SerializeToString(env.mutable_payload());
    return env.SerializeAsString();
}

std::string make_voice_self_status(bool connected, bool is_owner,
                                   const std::optional<HubId>& hub_id,
                                   const std::optional<ChannelId>& channel_id) {
    sercom::protocol::event::VoiceSelfStatus status;
    status.set_connected(connected);
    status.set_is_owner(is_owner);
    if (connected && hub_id.has_value() && channel_id.has_value()) {
        *status.mutable_channel() = to_proto_channel_ref(*hub_id, *channel_id);
    }
    return serialize_as_envelope(sercom::protocol::Envelope::VOICE_SELF_STATUS, status);
}

std::string make_presence_signal(const HubId& hub_id, const UserId& user_id, bool is_online) {
    sercom::protocol::event::RtSignal signal;
    auto* presence = signal.mutable_presence();
    presence->set_hub_id(hub_id.value);
    presence->set_user_id(user_id.value);
    presence->set_is_online(is_online);
    return make_rt_signal(signal);
}

}  // namespace

std::vector<net::outbound::OutgoingMessage> BootstrapCommand::execute(CommandContext& ctx,
                                                                      const queue::Event& evt) {
    std::vector<net::outbound::OutgoingMessage> out;
    const auto* event = std::get_if<queue::ConnectionEvent>(&evt);
    if (!event) {
        return out;
    }

    const UserId user_id = event->user_id;
    if (user_id.value.empty()) {
        out.emplace_back(make_drop_connection(
            event->conn_id,
            sercom::protocol::event::CommandErrorCode::CommandErrorCode_UNAUTHORIZED,
            "Authenticate first"));
        return out;
    }

    auto session_id_exp = ctx.session_manager.sessionIdOfConnection(event->conn_id);
    if (!session_id_exp.has_value()) {
        out.emplace_back(make_drop_connection(
            event->conn_id,
            sercom::protocol::event::CommandErrorCode::CommandErrorCode_UNAUTHORIZED,
            "Session not found"));
        return out;
    }
    const SessionId session_id = session_id_exp.value();

    const auto sync = build_state_sync_for_user(ctx, user_id);
    std::vector<HubId> hub_ids;
    hub_ids.reserve(sync.hubs_size());
    for (const auto& hub_sync : sync.hubs()) {
        const auto& hub_proto = hub_sync.hub();
        if (hub_proto.id().empty()) {
            continue;
        }
        HubId hub_id{hub_proto.id()};
        hub_ids.push_back(hub_id);
        ctx.subscription_manager.subscribeConnection(event->conn_id, Topic::HubTopic(hub_id));
    }

    out.emplace_back(make_outgoing_message(net::outbound::Target::one(event->conn_id),
                                           make_state_sync(sync)));

    std::optional<HubId> voice_hub_id;
    std::optional<ChannelId> voice_channel_id;
    const auto voice_channel = ctx.voice_service.sessions().user_channel(user_id);
    const auto voice_owner = ctx.voice_service.sessions().user_session(user_id);
    if (voice_channel.has_value()) {
        voice_channel_id = voice_channel;
        if (auto channel = ctx.hub_service.getChannel(*voice_channel)) {
            voice_hub_id = channel->hub_id;
        }
    }

    out.emplace_back(make_outgoing_message(
        net::outbound::Target::one(event->conn_id),
        make_voice_self_status(voice_channel.has_value(),
                               voice_owner.has_value() && *voice_owner == session_id, voice_hub_id,
                               voice_channel_id)));

    // Broadcast online presence only on the first active connection of this user.
    const auto user_conns = ctx.session_manager.getSessionConnections(user_id);
    if (user_conns.size() != 1) {
        return out;
    }

    const std::unordered_set<GlobalConnId> own_conn_set(user_conns.begin(), user_conns.end());
    for (const auto& hub_id : hub_ids) {
        utils::metrics::counters().fanout_subscriber_snapshot_total.fetch_add(
            1, std::memory_order_relaxed);
        auto subs = ctx.subscription_manager.getSubscribers(Topic::HubTopic(hub_id));
        if (!subs || subs->empty()) {
            continue;
        }

        std::vector<GlobalConnId> targets;
        targets.reserve(subs->size());
        for (const auto& conn : *subs) {
            if (own_conn_set.contains(conn)) {
                continue;
            }
            targets.push_back(conn);
        }
        if (targets.empty()) {
            continue;
        }

        out.emplace_back(make_outgoing_message(net::outbound::Target::many(std::move(targets)),
                                               make_presence_signal(hub_id, user_id, true)));
    }

    return out;
}

}  // namespace app
