#include "app/services/voice/VoiceService.h"

#include "app/commands/utils.h"
#include "app/managers/subscription/Topic.h"
#include "livekit/cli/LivekitClient.h"
#include "proto/envelope.pb.h"
#include "proto/event/activity.pb.h"
#include "utils/EnvLoader.h"
#include "utils/EventLogger.h"
#include "utils/Metrics.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace app::services::voice {
namespace {
using json = nlohmann::json;

uint64_t unix_now_seconds() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
                                       std::chrono::system_clock::now().time_since_epoch())
                                       .count());
}

std::string make_voice_channel_participants(
    const HubId& hub, const ChannelId& channel,
    const std::vector<VoiceSessionManager::ParticipantInfo>& channel_participants,
    uint64_t started_at_unix) {
    sercom::protocol::event::VoiceChannelParticipants participants;
    participants.mutable_channel()->set_hub_id(hub.value);
    participants.mutable_channel()->set_channel_id(channel.value);
    participants.set_started_at_unix(started_at_unix);
    for (const auto& p : channel_participants) {
        auto* out_p = participants.add_participants();
        out_p->set_user_id(p.user_id.value);
        out_p->set_muted(p.muted);
        out_p->set_deafened(p.deafened);
    }

    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(sercom::protocol::Envelope::VOICE_CHANNEL_PARTICIPANTS);
    participants.SerializeToString(env.mutable_payload());

    std::string bytes;
    env.SerializeToString(&bytes);
    return bytes;
}

std::string make_voice_activity(std::string_view hub_id, std::string_view channel_id,
                                std::string_view user_id, bool muted, bool deafened,
                                sercom::protocol::event::VoiceChannelActivityState state) {
    sercom::protocol::event::VoiceActivity activity;
    activity.mutable_channel()->set_hub_id(hub_id.data(), hub_id.size());
    activity.mutable_channel()->set_channel_id(channel_id.data(), channel_id.size());
    activity.mutable_participant()->set_user_id(user_id.data(), user_id.size());
    activity.mutable_participant()->set_muted(muted);
    activity.mutable_participant()->set_deafened(deafened);
    activity.set_activity(state);

    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(sercom::protocol::Envelope::VOICE_ACTIVITY_EVENT);
    activity.SerializeToString(env.mutable_payload());

    std::string bytes;
    env.SerializeToString(&bytes);
    return bytes;
}

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

sercom::protocol::event::VoiceChannelActivityState to_proto_presence_state(
    VoiceService::PresenceState state) {
    switch (state) {
        case VoiceService::PresenceState::Joined:
            return sercom::protocol::event::ACTIVITY_JOINED;
        case VoiceService::PresenceState::Left:
            return sercom::protocol::event::ACTIVITY_LEFT;
        default:
            return sercom::protocol::event::ACTIVITY_UNSPECIFIED;
    }
}

std::string make_voice_self_status_connected(bool is_owner, const ChannelId& channel) {
    sercom::protocol::event::VoiceSelfStatus status;
    status.set_connected(true);
    status.set_is_owner(is_owner);
    status.set_channel_id(channel.value);

    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(sercom::protocol::Envelope::VOICE_SELF_STATUS);
    status.SerializeToString(env.mutable_payload());

    std::string bytes;
    env.SerializeToString(&bytes);
    return bytes;
}

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

bool contains_participant(const std::vector<livekit::cli::ParticipantInfo>& participants,
                          const UserId& user) {
    return std::any_of(participants.begin(), participants.end(),
                       [&](const auto& p) { return p.identity == user; });
}

std::string generate_nonce_hex() {
    thread_local std::mt19937_64 rng(std::random_device{}());
    constexpr char kHex[] = "0123456789abcdef";

    std::string out;
    out.resize(32);
    for (size_t i = 0; i < 16; ++i) {
        const uint8_t byte = static_cast<uint8_t>(rng() & 0xFFu);
        out[i * 2] = kHex[(byte >> 4) & 0x0Fu];
        out[i * 2 + 1] = kHex[byte & 0x0Fu];
    }
    return out;
}

}  // namespace

VoiceService::VoiceService(std::string api_key, std::string api_secret,
                           VoiceStateRepository& voice_state_repo,
                           infra::redis::RedisClient& redis, SessionManager& session_manager,
                           SubscriptionManager& subscription_manager,
                           app::services::ChannelService& channel_service,
                           net::outbound::IOutboundSink& outbound_sink)
    : voice_state_repo_(voice_state_repo),
      redis_(redis),
      session_manager_(session_manager),
      subscription_manager_(subscription_manager),
      channel_service_(channel_service),
      outbound_sink_(outbound_sink),
      token_service_(std::move(api_key), std::move(api_secret)) {
    auto public_base = utils::EnvLoader::get_env("WEB_DOMAIN", "https://localhost");
    if (!public_base.empty() && public_base.back() == '/') {
        public_base.pop_back();
    }

    const auto node1_private =
        utils::EnvLoader::get_env("LIVEKIT_NODE1_PRIVATE_URL", "http://livekit-node1:7880");
    const auto node2_private =
        utils::EnvLoader::get_env("LIVEKIT_NODE2_PRIVATE_URL", "http://livekit-node2:7890");

    const auto node1_public =
        utils::EnvLoader::get_env("LIVEKIT_NODE1_PUBLIC_URL", public_base + "/livekit-node-1");
    const auto node2_public =
        utils::EnvLoader::get_env("LIVEKIT_NODE2_PUBLIC_URL", public_base + "/livekit-node-2");

    struct NodeConf {
        std::string id, priv, pub;
    };
    const NodeConf node_conf[] = {
        {"livekit-node1", node1_private, node1_public},
        {"livekit-node2", node2_private, node2_public},
    };

    for (const auto& n : node_conf) {
        nodes_.register_node({n.id, n.pub, n.priv});
    }
}

VoiceService::JoinVoiceToken VoiceService::join_voice(const ChannelId& channel, const UserId& user,
                                                      SessionId app_session_id,
                                                      std::string_view intent_nonce) {
    JoinVoiceToken response;

    auto node = nodes_.get_room_node(channel);
    if (!node) {
        node = nodes_.pick_node();
        if (node) nodes_.bind_room(channel, node->node_id);
    }

    if (!node) return response;

    livekit::LiveKitTokenService::ParticipantTokenRequest req{.identity = user,
                                                              .room = channel,
                                                              .node_id = node->node_id,
                                                              .app_session_id = app_session_id,
                                                              .intent_nonce =
                                                                  std::string(intent_nonce),
                                                              .can_publish = true,
                                                              .can_subscribe = true,
                                                              .ttl = std::chrono::minutes(10)};

    const std::string token = token_service_.mint_participant_token(req);
    const std::string key = e2ee_keys_.get_or_create_key(channel);

    response.token = token;
    response.livekit_url = node->public_host;
    response.expires_in = 600;
    response.e2ee_key = key;

    return response;
}

bool VoiceService::stage_pending_join_intent(const UserId& user, const PendingJoinIntent& intent,
                                             uint64_t expires_in_seconds) {
    const auto ttl = std::chrono::seconds(expires_in_seconds + 10);

    try {
        json doc;
        doc["user_id"] = user.value;
        doc["session_id"] = intent.session_id;
        doc["intent_nonce"] = intent.intent_nonce;
        doc["to_channel"] = intent.to_channel.value;
        doc["from_channel"] = intent.has_from_channel ? intent.from_channel.value : "";
        doc["has_from_channel"] = intent.has_from_channel;
        doc["muted"] = intent.muted;
        doc["deafened"] = intent.deafened;
        doc["old_leave_seen"] = intent.old_leave_seen;
        doc["new_join_seen"] = intent.new_join_seen;
        doc["expires_at_unix"] = unix_now_seconds() + static_cast<uint64_t>(ttl.count());

        redis_.setex(pending_join_key(user), ttl, doc.dump());
        return true;
    } catch (const std::exception& ex) {
        utils::EventLogger::instance().log(utils::EventCategory::VOICE, user.value,
                                           "pending_join_write_failed", 0,
                                           std::string("error=") + ex.what());
        return false;
    }
}

void VoiceService::clear_pending_join_intent(const UserId& user) {
    try {
        redis_.del(pending_join_key(user));
    } catch (...) {
    }
}

std::optional<VoiceService::PendingJoinIntent> VoiceService::read_pending_join_intent(
    const UserId& user) const {
    try {
        const auto raw = redis_.get(pending_join_key(user));
        if (!raw.has_value()) return std::nullopt;

        const auto doc = json::parse(*raw, nullptr, false);
        if (!doc.is_object()) return std::nullopt;

        PendingJoinIntent intent;

        if (doc.contains("session_id")) {
            if (doc["session_id"].is_number_unsigned()) {
                intent.session_id = static_cast<SessionId>(doc["session_id"].get<uint64_t>());
            } else if (doc["session_id"].is_string()) {
                intent.session_id = static_cast<SessionId>(
                    std::stoull(doc["session_id"].get<std::string>()));
            }
        }

        if (doc.contains("intent_nonce") && doc["intent_nonce"].is_string()) {
            intent.intent_nonce = doc["intent_nonce"].get<std::string>();
        }

        if (!doc.contains("to_channel") || !doc["to_channel"].is_string()) {
            return std::nullopt;
        }
        intent.to_channel = ChannelId(doc["to_channel"].get<std::string>());

        if (doc.contains("has_from_channel") && doc["has_from_channel"].is_boolean()) {
            intent.has_from_channel = doc["has_from_channel"].get<bool>();
        }
        if (doc.contains("from_channel") && doc["from_channel"].is_string()) {
            const auto from = doc["from_channel"].get<std::string>();
            if (!from.empty()) {
                intent.from_channel = ChannelId(from);
                intent.has_from_channel = true;
            }
        }

        if (doc.contains("muted") && doc["muted"].is_boolean()) {
            intent.muted = doc["muted"].get<bool>();
        }
        if (doc.contains("deafened") && doc["deafened"].is_boolean()) {
            intent.deafened = doc["deafened"].get<bool>();
        }
        if (doc.contains("old_leave_seen") && doc["old_leave_seen"].is_boolean()) {
            intent.old_leave_seen = doc["old_leave_seen"].get<bool>();
        }
        if (doc.contains("new_join_seen") && doc["new_join_seen"].is_boolean()) {
            intent.new_join_seen = doc["new_join_seen"].get<bool>();
        }
        if (doc.contains("expires_at_unix") && doc["expires_at_unix"].is_number_unsigned()) {
            intent.expires_at_unix = doc["expires_at_unix"].get<uint64_t>();
        }

        return intent;
    } catch (...) {
        return std::nullopt;
    }
}

bool VoiceService::update_pending_join_intent(const UserId& user,
                                              const PendingJoinIntent& intent) {
    try {
        const auto ttl = redis_.ttl(pending_join_key(user));
        if (!ttl.has_value() || ttl->count() <= 0) {
            return false;
        }

        json doc;
        doc["user_id"] = user.value;
        doc["session_id"] = intent.session_id;
        doc["intent_nonce"] = intent.intent_nonce;
        doc["to_channel"] = intent.to_channel.value;
        doc["from_channel"] = intent.has_from_channel ? intent.from_channel.value : "";
        doc["has_from_channel"] = intent.has_from_channel;
        doc["muted"] = intent.muted;
        doc["deafened"] = intent.deafened;
        doc["old_leave_seen"] = intent.old_leave_seen;
        doc["new_join_seen"] = intent.new_join_seen;
        doc["expires_at_unix"] = intent.expires_at_unix;

        redis_.setex(pending_join_key(user), *ttl, doc.dump());
        return true;
    } catch (...) {
        return false;
    }
}

bool VoiceService::mark_webhook_event_seen(const std::string& event_id) {
    if (event_id.empty()) return true;

    try {
        return redis_.setnxex(webhook_seen_key(event_id), kWebhookDedupTtl, "1");
    } catch (const std::exception& ex) {
        utils::EventLogger::instance().log(utils::EventCategory::VOICE, "",
                                           "webhook_dedup_failed", 0,
                                           std::string("event_id=") + event_id +
                                               " error=" + ex.what());
        return true;
    }
}

bool VoiceService::verified_kick_user(const ChannelId& channel, const UserId& target) {
    auto node = nodes_.get_room_node(channel);
    if (!node) return true;

    livekit::cli::LivekitClient client(node->private_host, token_service_);

    if (client.RemoveParticipant(channel, target)) {
        return true;
    }

    std::vector<livekit::cli::ParticipantInfo> participants;
    try {
        participants = client.ListParticipants(channel);
    } catch (const std::exception& ex) {
        utils::EventLogger::instance().log(utils::EventCategory::VOICE, target.value,
                                           "kick_fallback_check_failed", 0,
                                           "channel=" + channel.value +
                                               " error=" + ex.what());
        return false;
    }

    if (contains_participant(participants, target)) {
        utils::EventLogger::instance().log(utils::EventCategory::VOICE, target.value,
                                           "kick_remove_failed", 0,
                                           "channel=" + channel.value);
        return false;
    }

    utils::EventLogger::instance().log(utils::EventCategory::VOICE, target.value,
                                       "kick_remove_fallback_absent", 0,
                                       "channel=" + channel.value);
    return true;
}

void VoiceService::on_channel_empty(const ChannelId& channel) { e2ee_keys_.clear_key(channel); }

void VoiceService::on_channel_finish(const ChannelId& channel) {
    try {
        voice_state_repo_.remove_channel(channel);
    } catch (...) {
    }

    nodes_.clear_room(channel);
    e2ee_keys_.clear_key(channel);
    clear_channel_started_at_unix(channel);
}

bool VoiceService::kick_user(const ChannelId& channel, const UserId& target) {
    auto node = nodes_.get_room_node(channel);
    if (!node) return false;

    livekit::cli::LivekitClient client(node->private_host, token_service_);
    return client.RemoveParticipant(channel, target);
}

void VoiceService::mark_channel_takeover(const ChannelId& channel) {
    std::lock_guard lock(takeover_guard_mutex_);
    channel_takeover_guard_[channel] = std::chrono::steady_clock::now() + kTakeoverGuardTtl;
}

void VoiceService::set_channel_started_at_unix(const ChannelId& channel, uint64_t started_at_unix) {
    std::lock_guard lock(channel_started_mutex_);
    channel_started_at_unix_[channel] = started_at_unix;
}

void VoiceService::clear_channel_started_at_unix(const ChannelId& channel) {
    std::lock_guard lock(channel_started_mutex_);
    channel_started_at_unix_.erase(channel);
}

uint64_t VoiceService::read_channel_started_at_unix(const ChannelId& channel) const {
    std::lock_guard lock(channel_started_mutex_);
    if (const auto it = channel_started_at_unix_.find(channel); it != channel_started_at_unix_.end()) {
        return it->second;
    }
    return 0;
}

uint64_t VoiceService::channel_started_at_unix(const ChannelId& channel) const {
    return read_channel_started_at_unix(channel);
}

bool VoiceService::consume_channel_takeover_guard(const ChannelId& channel) {
    std::lock_guard lock(takeover_guard_mutex_);
    auto it = channel_takeover_guard_.find(channel);
    if (it == channel_takeover_guard_.end()) return false;

    if (it->second < std::chrono::steady_clock::now()) {
        channel_takeover_guard_.erase(it);
        return false;
    }

    channel_takeover_guard_.erase(it);
    return true;
}

void VoiceService::persist_voice_join(const UserId& user, const ChannelId& channel, bool muted,
                                      bool deafened) {
    try {
        voice_state_repo_.upsert(user, channel, muted, deafened);
    } catch (const std::exception& ex) {
        utils::EventLogger::instance().log(utils::EventCategory::VOICE, user.value,
                                           "persist_join_failed", 0,
                                           "channel=" + channel.value +
                                               " error=" + ex.what());
    }
}

void VoiceService::persist_voice_leave(const UserId& user) {
    try {
        voice_state_repo_.remove(user);
    } catch (const std::exception& ex) {
        utils::EventLogger::instance().log(utils::EventCategory::VOICE, user.value,
                                           "persist_leave_failed", 0,
                                           std::string("error=") + ex.what());
    }
}

void VoiceService::emit(net::outbound::OutgoingMessage msg) {
    (void)outbound_sink_.push(std::move(msg));
    utils::metrics::counters().outbound_msgs_total.fetch_add(1, std::memory_order_relaxed);
}

void VoiceService::publish_participants(const HubId& hub, const ChannelId& channel,
                                        uint64_t started_at_unix) {
    utils::metrics::counters().fanout_subscriber_snapshot_total.fetch_add(1,
                                                                           std::memory_order_relaxed);
    auto subs = subscription_manager_.getSubscribers(Topic::HubTopic(hub));
    if (!subs || subs->empty()) return;

    auto participants = sessions_.participants_in_channel(channel);
    std::vector<GlobalConnId> conns{subs->begin(), subs->end()};
    auto msg = ::app::make_outgoing_message(
        net::outbound::Target::many(std::move(conns)),
        make_voice_channel_participants(hub, channel, participants, started_at_unix));
    emit(std::move(msg));
}

void VoiceService::publish_presence(const HubId& hub, const ChannelId& channel, const UserId& user,
                                    PresenceState state, bool muted, bool deafened) {
    utils::metrics::counters().fanout_subscriber_snapshot_total.fetch_add(1,
                                                                           std::memory_order_relaxed);
    auto subs = subscription_manager_.getSubscribers(Topic::HubTopic(hub));
    if (!subs || subs->empty()) return;

    std::vector<GlobalConnId> conns{subs->begin(), subs->end()};
    auto msg = ::app::make_outgoing_message(
        net::outbound::Target::many(std::move(conns)),
        make_voice_activity(hub.value, channel.value, user.value, muted, deafened,
                            to_proto_presence_state(state)));
    emit(std::move(msg));
}

void VoiceService::publish_voice_state(const HubId& hub, const UserId& user, bool muted,
                                       bool deafened) {
    utils::metrics::counters().fanout_subscriber_snapshot_total.fetch_add(1,
                                                                           std::memory_order_relaxed);
    auto subs = subscription_manager_.getSubscribers(Topic::HubTopic(hub));
    if (!subs || subs->empty()) return;

    std::vector<GlobalConnId> conns{subs->begin(), subs->end()};
    auto msg = ::app::make_outgoing_message(net::outbound::Target::many(std::move(conns)),
                                            make_voice_state(user.value, muted, deafened));
    emit(std::move(msg));
}

void VoiceService::publish_self_status(const UserId& user, bool connected,
                                       const std::optional<SessionId>& owner_session_id,
                                       const std::optional<ChannelId>& channel,
                                       std::optional<SessionId> only_session_id) {
    for (const auto& session_id : session_manager_.getUserSessionIds(user)) {
        if (only_session_id.has_value() && *only_session_id != session_id) continue;

        auto session_conns = session_manager_.getSessionIdConnections(session_id);
        if (session_conns.empty()) continue;

        const bool is_owner =
            connected && owner_session_id.has_value() && owner_session_id.value() == session_id;

        std::string bytes;
        if (connected && channel.has_value()) {
            bytes = make_voice_self_status_connected(is_owner, *channel);
        } else {
            bytes = make_voice_self_status_disconnected();
        }

        auto msg =
            ::app::make_outgoing_message(net::outbound::Target::many(std::move(session_conns)),
                                         std::move(bytes));
        emit(std::move(msg));
    }
}

void VoiceService::handle_participant_joined(const livekit::webhook::LiveKitEvent& event) {
    const auto pending_opt = read_pending_join_intent(event.user_id);

    PendingJoinIntent intent;
    bool has_correlated_intent = false;

    if (pending_opt.has_value()) {
        intent = *pending_opt;
        if (event.channel_id == intent.to_channel) {
            if (event.app_session_id != 0 || !event.intent_nonce.empty()) {
                has_correlated_intent =
                    event.app_session_id == intent.session_id && !intent.intent_nonce.empty() &&
                    event.intent_nonce == intent.intent_nonce;
            } else {
                const auto sessions = session_manager_.getUserSessionIds(event.user_id);
                has_correlated_intent =
                    sessions.size() == 1 && sessions.front() == intent.session_id;
            }
        }
    } else if (event.app_session_id == 0 && event.intent_nonce.empty()) {
        const auto sessions = session_manager_.getUserSessionIds(event.user_id);
        if (sessions.size() == 1) {
            has_correlated_intent = true;
            intent.session_id = sessions.front();
            intent.to_channel = event.channel_id;
        }
    }

    if (!has_correlated_intent) {
        utils::EventLogger::instance().log(
            utils::EventCategory::VOICE, event.user_id.value, "join_intent_mismatch", 0,
            "event_channel=" + event.channel_id.value +
                " event_session=" + std::to_string(event.app_session_id));
        (void)kick_user(event.channel_id, event.user_id);
        return;
    }

    const bool first_join_in_channel = sessions_.join(event.channel_id, event.user_id, intent.session_id);
    (void)sessions_.set_deafened(event.user_id, intent.deafened);
    if (!intent.deafened) {
        (void)sessions_.set_muted(event.user_id, intent.muted);
    }

    if (first_join_in_channel) {
        const uint64_t started_at_unix =
            event.timestamp_ms > 0 ? (event.timestamp_ms / 1000) : unix_now_seconds();
        set_channel_started_at_unix(event.channel_id, started_at_unix);
    }

    bool final_muted = false;
    bool final_deafened = false;
    for (const auto& p : sessions_.participants_in_channel(event.channel_id)) {
        if (p.user_id == event.user_id) {
            final_muted = p.muted;
            final_deafened = p.deafened;
            break;
        }
    }

    persist_voice_join(event.user_id, event.channel_id, final_muted, final_deafened);
    utils::EventLogger::instance().voice_join(event.user_id.value, event.channel_id.value);

    if (auto channel = channel_service_.getChannel(event.channel_id)) {
        if (first_join_in_channel) {
            publish_participants(channel->hub_id, event.channel_id,
                                 read_channel_started_at_unix(event.channel_id));
        }
        publish_presence(channel->hub_id, event.channel_id, event.user_id, PresenceState::Joined,
                         final_muted, final_deafened);
        publish_voice_state(channel->hub_id, event.user_id, final_muted, final_deafened);
    }

    publish_self_status(event.user_id, true, intent.session_id, event.channel_id);

    if (pending_opt.has_value()) {
        if (intent.has_from_channel && intent.from_channel != intent.to_channel) {
            intent.new_join_seen = true;
            if (intent.old_leave_seen) {
                clear_pending_join_intent(event.user_id);
            } else {
                (void)update_pending_join_intent(event.user_id, intent);
            }
        } else {
            clear_pending_join_intent(event.user_id);
        }
    }
}

void VoiceService::handle_participant_left(const livekit::webhook::LiveKitEvent& event) {
    const auto pending_opt = read_pending_join_intent(event.user_id);

    const auto current_channel = sessions_.user_channel(event.user_id);
    const auto current_owner = sessions_.user_session(event.user_id);

    bool leaving_muted = false;
    bool leaving_deafened = false;
    for (const auto& p : sessions_.participants_in_channel(event.channel_id)) {
        if (p.user_id == event.user_id) {
            leaving_muted = p.muted;
            leaving_deafened = p.deafened;
            break;
        }
    }

    const bool matches_owner_session =
        event.app_session_id == 0 ||
        (current_owner.has_value() && *current_owner == event.app_session_id);

    const bool nonce_mismatch_to_pending =
        pending_opt.has_value() && pending_opt->to_channel == event.channel_id &&
        !pending_opt->intent_nonce.empty() && !event.intent_nonce.empty() &&
        event.intent_nonce != pending_opt->intent_nonce;

    const bool leaving_current_voice = current_channel.has_value() &&
                                       *current_channel == event.channel_id &&
                                       matches_owner_session &&
                                       !nonce_mismatch_to_pending;

    const bool old_leave_for_switch =
        pending_opt.has_value() && pending_opt->has_from_channel &&
        pending_opt->from_channel == event.channel_id && pending_opt->from_channel != pending_opt->to_channel;

    bool became_empty = false;
    if (leaving_current_voice || old_leave_for_switch) {
        became_empty = sessions_.leave(event.channel_id, event.user_id);
    }

    if (leaving_current_voice) {
        persist_voice_leave(event.user_id);
        utils::EventLogger::instance().voice_leave(event.user_id.value, event.channel_id.value);

        if (auto channel = channel_service_.getChannel(event.channel_id)) {
            publish_presence(channel->hub_id, event.channel_id, event.user_id,
                             PresenceState::Left, leaving_muted, leaving_deafened);
        }

        publish_self_status(event.user_id, false, std::nullopt, std::nullopt);

        if (became_empty) {
            clear_channel_started_at_unix(event.channel_id);
            on_channel_empty(event.channel_id);
        }

        if (old_leave_for_switch) {
            auto intent = *pending_opt;
            intent.old_leave_seen = true;
            if (intent.new_join_seen) {
                clear_pending_join_intent(event.user_id);
            } else {
                (void)update_pending_join_intent(event.user_id, intent);
            }
        }
        return;
    }

    if (old_leave_for_switch) {
        // Keep DB transitions visible even if old-leave arrives after new-join.
        persist_voice_leave(event.user_id);
        if (const auto active_channel = sessions_.user_channel(event.user_id);
            active_channel.has_value()) {
            bool active_muted = false;
            bool active_deafened = false;
            for (const auto& p : sessions_.participants_in_channel(*active_channel)) {
                if (p.user_id == event.user_id) {
                    active_muted = p.muted;
                    active_deafened = p.deafened;
                    break;
                }
            }
            persist_voice_join(event.user_id, *active_channel, active_muted, active_deafened);
        }

        if (auto channel = channel_service_.getChannel(event.channel_id)) {
            publish_presence(channel->hub_id, event.channel_id, event.user_id,
                             PresenceState::Left, leaving_muted, leaving_deafened);
        }

        if (sessions_.is_empty(event.channel_id)) {
            clear_channel_started_at_unix(event.channel_id);
            on_channel_empty(event.channel_id);
        }

        auto intent = *pending_opt;
        intent.old_leave_seen = true;
        if (intent.new_join_seen) {
            clear_pending_join_intent(event.user_id);
        } else {
            (void)update_pending_join_intent(event.user_id, intent);
        }
        return;
    }

    utils::EventLogger::instance().log(
        utils::EventCategory::VOICE, event.user_id.value, "ignored_participant_left", 0,
        "channel=" + event.channel_id.value +
            " event_session=" + std::to_string(event.app_session_id));
}

void VoiceService::recover_from_restart() {
    {
        std::lock_guard lock(channel_started_mutex_);
        channel_started_at_unix_.clear();
    }

    try {
        voice_state_repo_.clear_all();
    } catch (const std::exception& ex) {
        utils::EventLogger::instance().log(utils::EventCategory::VOICE, "", "recovery_db_failed",
                                           0, std::string("error=") + ex.what());
    }

    const auto nodes = nodes_.list_nodes();

    for (const auto& node : nodes) {
        livekit::cli::LivekitClient client(node.private_host, token_service_);

        std::vector<ChannelId> rooms;
        try {
            rooms = client.ListRooms();
        } catch (const std::exception& ex) {
            utils::EventLogger::instance().log(utils::EventCategory::VOICE, "",
                                               "recovery_list_rooms_failed", 0,
                                               "node=" + node.node_id + " error=" + ex.what());
            continue;
        }

        for (const auto& room : rooms) {
            std::vector<livekit::cli::ParticipantInfo> participants;
            try {
                participants = client.ListParticipants(room);
            } catch (const std::exception& ex) {
                utils::EventLogger::instance().log(utils::EventCategory::VOICE, "",
                                                   "recovery_list_participants_failed", 0,
                                                   "room=" + room.value + " error=" + ex.what());
                continue;
            }

            for (const auto& p : participants) {
                client.RemoveParticipant(room, p.identity);
            }
        }
    }
}

void VoiceService::on_livekit_event(const livekit::webhook::LiveKitEvent& event) {
    using Type = livekit::webhook::LiveKitEventType;

    if (!mark_webhook_event_seen(event.event_id)) {
        utils::EventLogger::instance().log(utils::EventCategory::VOICE, event.user_id.value,
                                           "duplicate_webhook_event", 0,
                                           "event_id=" + event.event_id);
        return;
    }

    auto bound_node = nodes_.get_room_node(event.channel_id);
    const std::string current_node_id = bound_node ? bound_node->node_id : "";

    switch (event.type) {
        case Type::ROOM_STARTED:
            if (!current_node_id.empty()) nodes_.increment_room(current_node_id);
            break;

        case Type::ROOM_FINISHED:
            if (!current_node_id.empty()) nodes_.decrement_room(current_node_id);

            if (consume_channel_takeover_guard(event.channel_id)) {
                utils::EventLogger::instance().log(utils::EventCategory::VOICE, "",
                                                   "room_finished_guarded", 0,
                                                   "channel=" + event.channel_id.value);
                break;
            }

            on_channel_finish(event.channel_id);
            break;

        case Type::PARTICIPANT_JOINED: {
            if (!event.node_id.empty()) {
                nodes_.bind_room(event.channel_id, event.node_id);
                nodes_.increment_user(event.node_id);
            } else if (!current_node_id.empty()) {
                nodes_.increment_user(current_node_id);
            }

            handle_participant_joined(event);
            break;
        }

        case Type::PARTICIPANT_LEFT:
        case Type::PARTICIPANT_CONNECTION_ABORTED: {
            if (!event.node_id.empty() && !current_node_id.empty() &&
                event.node_id != current_node_id) {
                utils::EventLogger::instance().log(
                    utils::EventCategory::VOICE, event.user_id.value, "stale_participant_left", 0,
                    "event_node=" + event.node_id + " current_node=" + current_node_id +
                        " channel=" + event.channel_id.value);
                nodes_.decrement_user(event.node_id);
                break;
            }

            if (!event.node_id.empty()) {
                nodes_.decrement_user(event.node_id);
            } else if (!current_node_id.empty()) {
                nodes_.decrement_user(current_node_id);
            }

            handle_participant_left(event);
            break;
        }

        case Type::UNKNOWN:
        default:
            break;
    }
}

std::string VoiceService::generate_intent_nonce() const { return generate_nonce_hex(); }

std::string VoiceService::pending_join_key(const UserId& user) {
    return "voice:pending_join:" + user.value;
}

std::string VoiceService::webhook_seen_key(const std::string& event_id) {
    return "voice:webhook_seen:" + event_id;
}

}  // namespace app::services::voice
