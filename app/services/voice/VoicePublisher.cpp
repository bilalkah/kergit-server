#include "app/services/voice/VoicePublisher.h"

#include "app/commands/utils.h"
#include "app/managers/subscription/Topic.h"
#include "proto/envelope.pb.h"
#include "proto/event/activity.pb.h"
#include "proto/event/state.pb.h"
#include "utils/Metrics.h"

#include <utility>
#include <vector>

namespace app::services::voice {
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

std::string make_voice_self_status_connected(bool is_owner, const HubId& hub_id,
                                             const ChannelId& channel_id,
                                             const std::optional<std::string>& resume_id) {
    sercom::protocol::event::VoiceSelfStatus status;
    status.set_connected(true);
    status.set_is_owner(is_owner);
    *status.mutable_channel() = ::app::to_proto_channel_ref(hub_id, channel_id);
    if (is_owner && resume_id.has_value() && !resume_id->empty()) {
        status.set_resume_id(*resume_id);
    }
    return serialize_as_envelope(sercom::protocol::Envelope::VOICE_SELF_STATUS, status);
}

std::string make_voice_self_status_disconnected() {
    sercom::protocol::event::VoiceSelfStatus status;
    status.set_connected(false);
    status.set_is_owner(false);
    return serialize_as_envelope(sercom::protocol::Envelope::VOICE_SELF_STATUS, status);
}

std::string make_voice_key_update(const HubId& hub_id, const ChannelId& channel_id,
                                  const std::string& key, uint32_t key_index) {
    sercom::protocol::event::VoiceKeyUpdate update;
    *update.mutable_channel() = ::app::to_proto_channel_ref(hub_id, channel_id);
    update.set_e2ee_key(key);
    update.set_key_index(key_index);
    return serialize_as_envelope(sercom::protocol::Envelope::VOICE_KEY_UPDATE, update);
}

}  // namespace

VoicePublisher::VoicePublisher(net::outbound::IOutboundSink& outbound_sink,
                               SubscriptionManager& subscription_manager,
                               SessionManager& session_manager,
                               app::services::HubService& hub_service, VoiceSessionManager& sessions,
                               VoiceResumeRegistry& resume_registry)
    : outbound_sink_(outbound_sink),
      subscription_manager_(subscription_manager),
      session_manager_(session_manager),
      hub_service_(hub_service),
      sessions_(sessions),
      resume_registry_(resume_registry) {}

void VoicePublisher::emit(net::outbound::OutgoingMessage msg) {
    (void)outbound_sink_.push(std::move(msg));
    utils::metrics::counters().outbound_msgs_total.fetch_add(1, std::memory_order_relaxed);
}

void VoicePublisher::publish_voice_snapshot(const HubId& hub, const ChannelId& channel,
                                            uint64_t started_at_unix) {
    utils::metrics::counters().fanout_subscriber_snapshot_total.fetch_add(
        1, std::memory_order_relaxed);
    auto subs = subscription_manager_.getSubscribers(Topic::HubTopic(hub));
    if (!subs || subs->empty()) return;

    sercom::protocol::event::StateDelta delta;
    auto* hub_delta = delta.add_hubs();
    hub_delta->set_hub_id(hub.value);
    auto* channel_delta = hub_delta->add_channels();
    channel_delta->set_channel_id(channel.value);
    auto* snapshot = channel_delta->add_voice_ops()->mutable_snapshot()->mutable_state();
    snapshot->set_started_at_unix(started_at_unix);
    for (const auto& participant : sessions_.participants_in_channel(channel)) {
        auto* out_participant = snapshot->add_participants();
        out_participant->set_user_id(participant.user_id.value);
        out_participant->set_muted(participant.muted);
        out_participant->set_deafened(participant.deafened);
    }

    std::vector<GlobalConnId> conns{subs->begin(), subs->end()};
    auto msg = ::app::make_outgoing_message(net::outbound::Target::many(std::move(conns)),
                                            ::app::make_state_delta(delta));
    emit(std::move(msg));
}

void VoicePublisher::publish_voice_participant_upsert(const HubId& hub, const ChannelId& channel,
                                                      const UserId& user, bool muted,
                                                      bool deafened) {
    utils::metrics::counters().fanout_subscriber_snapshot_total.fetch_add(
        1, std::memory_order_relaxed);
    auto subs = subscription_manager_.getSubscribers(Topic::HubTopic(hub));
    if (!subs || subs->empty()) return;

    sercom::protocol::event::StateDelta delta;
    auto* hub_delta = delta.add_hubs();
    hub_delta->set_hub_id(hub.value);
    auto* channel_delta = hub_delta->add_channels();
    channel_delta->set_channel_id(channel.value);
    auto* upsert = channel_delta->add_voice_ops()->mutable_upsert()->mutable_participant();
    upsert->set_user_id(user.value);
    upsert->set_muted(muted);
    upsert->set_deafened(deafened);

    std::vector<GlobalConnId> conns{subs->begin(), subs->end()};
    auto msg = ::app::make_outgoing_message(net::outbound::Target::many(std::move(conns)),
                                            ::app::make_state_delta(delta));
    emit(std::move(msg));
}

void VoicePublisher::publish_voice_participant_remove(const HubId& hub, const ChannelId& channel,
                                                      const UserId& user) {
    utils::metrics::counters().fanout_subscriber_snapshot_total.fetch_add(
        1, std::memory_order_relaxed);
    auto subs = subscription_manager_.getSubscribers(Topic::HubTopic(hub));
    if (!subs || subs->empty()) return;

    sercom::protocol::event::StateDelta delta;
    auto* hub_delta = delta.add_hubs();
    hub_delta->set_hub_id(hub.value);
    auto* channel_delta = hub_delta->add_channels();
    channel_delta->set_channel_id(channel.value);
    channel_delta->add_voice_ops()->mutable_remove()->set_user_id(user.value);

    std::vector<GlobalConnId> conns{subs->begin(), subs->end()};
    auto msg = ::app::make_outgoing_message(net::outbound::Target::many(std::move(conns)),
                                            ::app::make_state_delta(delta));
    emit(std::move(msg));
}

void VoicePublisher::publish_voice_key_update(const HubId& hub, const ChannelId& channel,
                                              const std::string& key, uint32_t key_index) {
    // Target each participant's owner session (the one holding the LiveKit transport),
    // not hub subscribers — only active voice members need the key.
    for (const auto& participant : sessions_.participants_in_channel(channel)) {
        publish_voice_key_update_to_user(hub, channel, participant.user_id, key, key_index);
    }
}

void VoicePublisher::publish_voice_key_update_to_user(const HubId& hub, const ChannelId& channel,
                                                      const UserId& user, const std::string& key,
                                                      uint32_t key_index) {
    const auto owner_session = sessions_.user_session(user);
    if (!owner_session.has_value()) return;

    auto conns = session_manager_.getSessionIdConnections(*owner_session);
    if (conns.empty()) return;

    auto msg = ::app::make_outgoing_message(net::outbound::Target::many(std::move(conns)),
                                            make_voice_key_update(hub, channel, key, key_index));
    emit(std::move(msg));
}

void VoicePublisher::publish_self_status(const UserId& user, bool connected,
                                         const std::optional<SessionId>& owner_session_id,
                                         const std::optional<ChannelId>& channel,
                                         std::optional<SessionId> only_session_id) {
    std::optional<HubId> hub_id;
    if (connected && channel.has_value()) {
        if (const auto channel_info = hub_service_.getChannel(*channel)) {
            hub_id = channel_info->hub_id;
        }
    }

    const auto resume_id = connected ? resume_registry_.read(user) : std::nullopt;

    for (const auto& session_id : session_manager_.getUserSessionIds(user)) {
        if (only_session_id.has_value() && *only_session_id != session_id) continue;

        auto session_conns = session_manager_.getSessionIdConnections(session_id);
        if (session_conns.empty()) continue;

        const bool is_owner =
            connected && owner_session_id.has_value() && owner_session_id.value() == session_id;

        std::string bytes = make_voice_self_status_disconnected();
        if (connected && channel.has_value() && hub_id.has_value()) {
            bytes = make_voice_self_status_connected(is_owner, *hub_id, *channel,
                                                     is_owner ? resume_id : std::nullopt);
        }

        auto msg = ::app::make_outgoing_message(
            net::outbound::Target::many(std::move(session_conns)), std::move(bytes));
        emit(std::move(msg));
    }
}

}  // namespace app::services::voice
