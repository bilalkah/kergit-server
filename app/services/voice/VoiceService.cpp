#include "app/services/voice/VoiceService.h"

#include "app/commands/utils.h"
#include "app/managers/subscription/Topic.h"
#include "app/services/voice/ChannelKeyService.h"
#include "app/services/voice/VoiceNonce.h"
#include "livekit/cli/LivekitClient.h"
#include "utils/EnvLoader.h"
#include "utils/EventLogger.h"
#include "utils/Metrics.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace app::services::voice {
namespace {
using json = nlohmann::json;

uint64_t unix_now_seconds() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
                                     std::chrono::system_clock::now().time_since_epoch())
                                     .count());
}

bool contains_participant(const std::vector<livekit::cli::ParticipantInfo>& participants,
                          const UserId& user) {
    return std::any_of(participants.begin(), participants.end(),
                       [&](const auto& p) { return p.identity == user; });
}

SessionId next_recovery_session_id() {
    static std::atomic<uint64_t> counter{1};
    constexpr uint64_t kRecoverySessionPrefix = 1ull << 63;
    return kRecoverySessionPrefix | counter.fetch_add(1, std::memory_order_relaxed);
}

}  // namespace

VoiceService::VoiceService(std::string api_key, std::string api_secret,
                           infra::redis::RedisClient& redis, SessionManager& session_manager,
                           SubscriptionManager& subscription_manager,
                           app::services::HubService& hub_service,
                           net::outbound::IOutboundSink& outbound_sink,
                           const std::vector<core::LivekitNodeConfig>& nodes,
                           std::string livekit_cluster_url)
    : redis_(redis),
      session_manager_(session_manager),
      subscription_manager_(subscription_manager),
      hub_service_(hub_service),
      outbound_sink_(outbound_sink),
      token_service_(std::move(api_key), std::move(api_secret)),
      livekit_cluster_url_(std::move(livekit_cluster_url)),
      pending_intents_(redis),
      resume_registry_(redis),
      publisher_(outbound_sink_, subscription_manager_, session_manager_, hub_service_, sessions_,
                 resume_registry_),
      channel_keys_(redis_, nodes_, token_service_, sessions_, publisher_, hub_service_),
      reconciler_(*this) {
    for (const auto& node : nodes) {
        nodes_.register_node({node.id, node.public_url, node.private_url});
    }
}

VoiceService::~VoiceService() { stop_reconcile_loop(); }

void VoiceService::start_reconcile_loop() { reconciler_.start(); }

void VoiceService::stop_reconcile_loop() { reconciler_.stop(); }

void VoiceService::reconcile_channel(const ChannelId& channel, std::string_view reason) {
    reconcile_channel_state(channel, reason);
}

void VoiceService::reconcile_full(std::string_view reason) { reconcile_full_state(reason); }

VoiceService::JoinVoiceToken VoiceService::join_voice(const ChannelId& channel, const UserId& user,
                                                      SessionId app_session_id,
                                                      std::string_view intent_nonce) {
    JoinVoiceToken response;

    if (channel_keys_.rekey_blocks_join(channel)) {
        response.error_reason = "voice_rekey_in_progress";
        return response;
    }

    // We no longer pick/pin a node ourselves. The cluster is one logical LiveKit (shared
    // Redis); the client connects to the single load-balanced endpoint and LiveKit decides
    // which node hosts the room. We only need a configured node to exist.
    auto node = nodes_.any_node();
    if (!node) return response;
    reconciler_.clear_channel_remote_missing_confirmation(channel);

    auto acquired = channel_keys_.acquire_for_join(channel, user);
    if (!acquired.key.has_value()) {
        response.error_reason = acquired.error_reason;
        return response;
    }
    const auto& ck = *acquired.key;

    livekit::LiveKitTokenService::ParticipantTokenRequest req{
        .identity = user,
        .room = channel,
        // node_id stays in participant metadata for diagnostics only; it no longer drives
        // routing (LiveKit owns host placement via shared Redis).
        .node_id = node->node_id,
        .app_session_id = app_session_id,
        .intent_nonce = std::string(intent_nonce),
        .can_publish = true,
        .can_subscribe = true,
        .ttl = std::chrono::minutes(10)};

    const std::string token = token_service_.mint_participant_token(req);

    response.token = token;
    response.livekit_url = livekit_cluster_url_;
    response.expires_in = 600;
    response.e2ee_key = ck.key;
    response.key_index = ck.key_index;
    response.resume_id = resume_registry_.rotate(user);

    return response;
}

bool VoiceService::stage_pending_join_intent(const UserId& user, const PendingJoinIntent& intent,
                                             uint64_t expires_in_seconds) {
    return pending_intents_.stage(user, intent, expires_in_seconds);
}

void VoiceService::clear_pending_join_intent(const UserId& user) { pending_intents_.clear(user); }

VoiceService::ParticipantMetadata VoiceService::parse_participant_metadata(
    std::string_view metadata_raw) {
    ParticipantMetadata out;
    if (metadata_raw.empty()) return out;

    const auto metadata = json::parse(std::string(metadata_raw), nullptr, false);
    if (!metadata.is_object()) {
        out.node_id = std::string(metadata_raw);
        return out;
    }

    out.structured = true;

    if (metadata.contains("node_id") && metadata["node_id"].is_string()) {
        out.node_id = metadata["node_id"].get<std::string>();
    }

    if (metadata.contains("app_session_id")) {
        if (metadata["app_session_id"].is_number_unsigned()) {
            out.app_session_id = metadata["app_session_id"].get<uint64_t>();
        } else if (metadata["app_session_id"].is_string()) {
            try {
                out.app_session_id = static_cast<uint64_t>(
                    std::stoull(metadata["app_session_id"].get<std::string>()));
            } catch (...) {
                out.app_session_id = 0;
            }
        }
    }

    if (metadata.contains("intent_nonce") && metadata["intent_nonce"].is_string()) {
        out.intent_nonce = metadata["intent_nonce"].get<std::string>();
    }

    return out;
}

std::optional<std::string> VoiceService::resolve_participant_node_assignment(
    const ChannelId& channel,
    const std::unordered_map<UserId, livekit::cli::ParticipantInfo>& participants,
    std::string_view reason) const {
    std::optional<std::string> resolved_node_id;
    for (const auto& [_, participant] : participants) {
        const auto metadata = parse_participant_metadata(participant.metadata);
        if (metadata.node_id.empty() || !nodes_.get_node(metadata.node_id)) continue;

        if (!resolved_node_id.has_value()) {
            resolved_node_id = metadata.node_id;
            continue;
        }
        if (*resolved_node_id == metadata.node_id) continue;

        utils::EventLogger::instance().log(
            utils::EventCategory::VOICE, "", "room_assignment_metadata_conflict", 0,
            "channel=" + channel.value + " first_node=" + *resolved_node_id +
                " conflicting_node=" + metadata.node_id + " reason=" + std::string(reason));
        return std::nullopt;
    }
    return resolved_node_id;
}

bool VoiceService::mark_webhook_event_seen(const std::string& event_id) {
    if (event_id.empty()) return true;

    try {
        return redis_.setnxex(webhook_seen_key(event_id), kWebhookDedupTtl, "1");
    } catch (const std::exception& ex) {
        utils::EventLogger::instance().log(
            utils::EventCategory::VOICE, "", "webhook_dedup_failed", 0,
            std::string("event_id=") + event_id + " error=" + ex.what());
        return true;
    }
}

bool VoiceService::verified_kick_user(const ChannelId& channel, const UserId& target) {
    auto node = nodes_.any_node();
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
                                           "channel=" + channel.value + " error=" + ex.what());
        return false;
    }

    if (contains_participant(participants, target)) {
        utils::EventLogger::instance().log(utils::EventCategory::VOICE, target.value,
                                           "kick_remove_failed", 0, "channel=" + channel.value);
        return false;
    }

    utils::EventLogger::instance().log(utils::EventCategory::VOICE, target.value,
                                       "kick_remove_fallback_absent", 0,
                                       "channel=" + channel.value);
    return true;
}

void VoiceService::on_channel_empty(const ChannelId& channel) {
    channel_keys_.clear_channel(channel);
    // Drop the room->node binding too: once our own state agrees the channel is
    // empty (last participant left via an authoritative LiveKit event), there is no
    // disagreement left to reconcile. Keeping the binding would make the reconcile
    // sweep treat the channel as still tracked and run a pointless missing-confirm
    // cycle whose only effect is clearing this binding. Forget it now instead.
    nodes_.clear_room(channel);
}

void VoiceService::on_channel_finish(const ChannelId& channel) {
    reconciler_.reset_remote_missing_confirmations(channel);

    const auto removed_users = sessions_.clear_channel(channel);
    const auto channel_info = hub_service_.getChannel(channel);
    for (const auto& user : removed_users) {
        if (channel_info.has_value()) {
            publisher_.publish_voice_participant_remove(channel_info->hub_id, channel, user);
        }
        publisher_.publish_self_status(user, false, std::nullopt, std::nullopt);
        resume_registry_.clear(user);
    }

    nodes_.clear_room(channel);
    channel_keys_.clear_channel(channel);
    clear_channel_started_at_unix(channel);
}

void VoiceService::force_local_leave(const UserId& user, const ChannelId& channel,
                                     std::string_view reason) {
    reconciler_.clear_participant_remote_missing_confirmation(channel, user);

    const bool became_empty = sessions_.leave(channel, user);

    utils::EventLogger::instance().log(
        utils::EventCategory::VOICE, user.value, "reconcile_forced_leave", 0,
        "channel=" + channel.value + " reason=" + std::string(reason));

    if (auto channel_info = hub_service_.getChannel(channel)) {
        publisher_.publish_voice_participant_remove(channel_info->hub_id, channel, user);
    }

    publisher_.publish_self_status(user, false, std::nullopt, std::nullopt);
    resume_registry_.clear(user);

    if (became_empty) {
        clear_channel_started_at_unix(channel);
        on_channel_empty(channel);
    } else {
        // Others remain → rotate so the force-removed member can't decrypt future audio.
        channel_keys_.rotate_and_broadcast(channel, "participant_force_left");
    }
}

bool VoiceService::kick_user(const ChannelId& channel, const UserId& target) {
    auto node = nodes_.any_node();
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
    if (const auto it = channel_started_at_unix_.find(channel);
        it != channel_started_at_unix_.end()) {
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

bool VoiceService::is_stale_participant_event(const livekit::webhook::LiveKitEvent& event) {
    if (event.timestamp_ms == 0 || event.user_id.value.empty()) return false;

    std::lock_guard lock(event_order_mutex_);
    auto& last_ts = user_last_event_ts_ms_[event.user_id];
    if (last_ts != 0 && event.timestamp_ms < last_ts) {
        return true;
    }
    if (event.timestamp_ms > last_ts) {
        last_ts = event.timestamp_ms;
    }
    return false;
}

bool VoiceService::is_stale_room_event(const livekit::webhook::LiveKitEvent& event) {
    if (event.timestamp_ms == 0 || event.channel_id.value.empty()) return false;

    std::lock_guard lock(event_order_mutex_);
    auto& last_ts = channel_last_room_event_ts_ms_[event.channel_id];
    if (last_ts != 0 && event.timestamp_ms < last_ts) {
        return true;
    }
    if (event.timestamp_ms > last_ts) {
        last_ts = event.timestamp_ms;
    }
    return false;
}

bool VoiceService::has_active_owner_connection(const UserId& user, const ChannelId& channel) const {
    const auto current_channel = sessions_.user_channel(user);
    if (!current_channel.has_value() || *current_channel != channel) {
        return false;
    }

    const auto owner_session = sessions_.user_session(user);
    return owner_session.has_value() && session_manager_.sessionIdHasConnections(*owner_session);
}

std::size_t VoiceService::active_owner_connection_count(const ChannelId& channel) const {
    std::size_t count = 0;
    for (const auto& participant : sessions_.participants_in_channel(channel)) {
        if (has_active_owner_connection(participant.user_id, channel)) {
            ++count;
        }
    }
    return count;
}

void VoiceService::reconcile_full_state(std::string_view reason) {
    std::unordered_set<ChannelId> channels;
    for (const auto& channel : sessions_.active_channels()) {
        channels.insert(channel);
    }
    for (const auto& channel : reconciler_.tracked_missing_channels()) {
        channels.insert(channel);
    }

    // One logical cluster: a single node answers for every room (shared Redis), so query
    // one node rather than fanning out. ListRooms surfaces rooms LiveKit knows about that we
    // may not track yet (e.g. a missed webhook); fold in any with participants or local state.
    if (auto node = nodes_.any_node()) {
        livekit::cli::LivekitClient client(node->private_host, token_service_);
        try {
            for (const auto& room : client.ListRooms()) {
                if (room.num_participants > 0 || !sessions_.is_empty(room.room)) {
                    channels.insert(room.room);
                }
            }
        } catch (const std::exception& ex) {
            utils::EventLogger::instance().log(
                utils::EventCategory::VOICE, "", "reconcile_list_rooms_failed", 0,
                "node=" + node->node_id + " method=ListRooms endpoint=" + node->private_host +
                    " error=" + ex.what());
        }
    }

    for (const auto& channel : channels) {
        reconcile_channel_state(channel, reason);
    }
}

void VoiceService::reconcile_channel_state(const ChannelId& channel, std::string_view reason) {
    std::unordered_map<UserId, livekit::cli::ParticipantInfo> remote_by_user;
    ReconcileQuerySummary query_summary;

    // Single cluster query: any node answers for any room via shared Redis. Fanning out to
    // every node was useless (N reads of the same Redis record → N identical answers) and
    // gave false confidence; one query is the cluster's answer.
    auto node = nodes_.any_node();
    const std::string query_private_host = node ? node->private_host : "";
    query_summary.configured_queries = node ? 1 : 0;

    if (node) {
        livekit::cli::LivekitClient client(query_private_host, token_service_);
        try {
            const auto participants = client.ListParticipants(channel);
            ++query_summary.successful_queries;
            query_summary.participant_count += participants.size();
            for (const auto& participant : participants) {
                if (participant.identity.value.empty()) continue;
                remote_by_user.emplace(participant.identity, participant);
            }
        } catch (const std::exception& ex) {
            ++query_summary.failed_queries;
            utils::EventLogger::instance().log(
                utils::EventCategory::VOICE, "", "reconcile_list_participants_failed", 0,
                "node=" + node->node_id + " method=ListParticipants endpoint=" +
                    query_private_host + " channel=" + channel.value + " error=" + ex.what());
        } catch (...) {
            ++query_summary.failed_queries;
            utils::EventLogger::instance().log(
                utils::EventCategory::VOICE, "", "reconcile_list_participants_failed", 0,
                "node=" + node->node_id + " method=ListParticipants endpoint=" +
                    query_private_host + " channel=" + channel.value + " error=unknown");
        }
    }
    const auto local_participants = sessions_.participants_in_channel(channel);
    const std::string query_details =
        " configured_queries=" + std::to_string(query_summary.configured_queries) +
        " successful_queries=" + std::to_string(query_summary.successful_queries) +
        " failed_queries=" + std::to_string(query_summary.failed_queries) +
        " participant_count=" + std::to_string(query_summary.participant_count);
    const bool all_queries_succeeded = query_summary.all_queries_succeeded();
    if (!all_queries_succeeded) {
        reconciler_.reset_remote_missing_confirmations(channel);
        utils::EventLogger::instance().log(
            utils::EventCategory::VOICE, "", "reconcile_participants_inconclusive", 0,
            "channel=" + channel.value + " reason=" + std::string(reason) + query_details);
    }

    if (remote_by_user.empty()) {
        if (!all_queries_succeeded) {
            return;
        }
        if (!query_summary.unanimously_absent()) {
            reconciler_.clear_channel_remote_missing_confirmation(channel);
            utils::EventLogger::instance().log(
                utils::EventCategory::VOICE, "", "reconcile_participants_unusable", 0,
                "channel=" + channel.value + " reason=" + std::string(reason) + query_details);
            return;
        }

        // Both sides agree the room is empty AND we hold no local state for it, so
        // there is nothing to tear down. Returning here avoids re-running the missing
        // confirmation cycle (and on_channel_finish) every interval against a room
        // LiveKit keeps listing while it lingers post-emptying.
        if (sessions_.is_empty(channel)) {
            reconciler_.clear_channel_remote_missing_confirmation(channel);
            return;
        }

        utils::EventLogger::instance().log(
            utils::EventCategory::VOICE, "", "reconcile_room_missing_remote", 0,
            "channel=" + channel.value + " reason=" + std::string(reason) + query_details);
        const auto active_owner_count = active_owner_connection_count(channel);
        if (active_owner_count > 0) {
            // Active owner connections are authoritative local state: these users' control
            // sessions are alive and we still track them as joined. A remote query reporting
            // the room empty contradicts that, and LiveKit membership is NOT ground truth — a
            // ListParticipants returning empty can be a node hiccup, a room that briefly moved
            // nodes, or a query that reached the wrong node (ListParticipants returns an empty
            // list rather than an error when a node does not host the room). NEVER tear down a
            // channel that still has active owners: doing so kicked whole channels of connected
            // users on a transient false-positive. Genuine individual departures are handled by
            // the per-participant participant_left path; here we hold and wait for owners to drop.
            reconciler_.clear_channel_remote_missing_confirmation(channel);
            utils::EventLogger::instance().log(
                utils::EventCategory::VOICE, "", "reconcile_room_missing_remote_held", 0,
                "channel=" + channel.value + " reason=" + std::string(reason) +
                    " active_owner_connections=" + std::to_string(active_owner_count) +
                    query_details);
            return;
        }
        if (reconciler_.confirm_channel_remote_missing(channel, reason)) {
            on_channel_finish(channel);
        }
        return;
    }

    reconciler_.clear_channel_remote_missing_confirmation(channel);

    std::unordered_set<UserId> local_users;
    for (const auto& participant : local_participants) {
        local_users.insert(participant.user_id);
    }

    std::unordered_set<UserId> remote_users;
    for (const auto& [user_id, _] : remote_by_user) {
        remote_users.insert(user_id);
    }

    if (all_queries_succeeded) {
        for (const auto& participant : local_participants) {
            if (remote_users.find(participant.user_id) != remote_users.end()) continue;
            if (has_active_owner_connection(participant.user_id, channel)) {
                // The user's control session is alive — that is authoritative. A poll absence
                // is NOT grounds to eject them: the client is our live sensor and a genuine
                // media drop arrives as a participant_left webhook. Hold; never force a leave
                // for a user who still holds a live owner connection.
                reconciler_.clear_participant_remote_missing_confirmation(channel,
                                                                         participant.user_id);
                utils::EventLogger::instance().log(
                    utils::EventCategory::VOICE, participant.user_id.value,
                    "reconcile_participant_missing_held", 0,
                    "channel=" + channel.value +
                        " reason=participant_missing_in_livekit active_owner_connection=1");
                continue;
            }

            // No active owner connection → the user is genuinely gone; safe to clean up.
            if (reconciler_.confirm_participant_remote_missing(channel, participant.user_id,
                                                   "participant_missing_in_livekit")) {
                force_local_leave(participant.user_id, channel, "participant_missing_in_livekit");
            }
        }
    }

    livekit::cli::LivekitClient remove_client(query_private_host, token_service_);
    for (const auto& [user_id, participant] : remote_by_user) {
        reconciler_.clear_participant_remote_missing_confirmation(channel, user_id);
        if (local_users.find(user_id) != local_users.end()) continue;

        const auto metadata = parse_participant_metadata(participant.metadata);
        const auto pending = pending_intents_.read(user_id);

        bool correlation_valid = false;
        if (pending.has_value() && pending->to_channel == channel) {
            const bool session_match =
                metadata.app_session_id != 0 &&
                static_cast<SessionId>(metadata.app_session_id) == pending->session_id;
            const bool nonce_match = !pending->intent_nonce.empty() &&
                                     !metadata.intent_nonce.empty() &&
                                     pending->intent_nonce == metadata.intent_nonce;
            correlation_valid = session_match && nonce_match;
        } else if (metadata.app_session_id != 0) {
            const auto sessions = session_manager_.getUserSessionIds(user_id);
            correlation_valid = std::any_of(sessions.begin(), sessions.end(), [&](SessionId sid) {
                return sid == static_cast<SessionId>(metadata.app_session_id);
            });
        }

        if (!correlation_valid) {
            bool removed = false;
            if (all_queries_succeeded && !query_private_host.empty()) {
                removed = remove_client.RemoveParticipant(channel, user_id);
            }
            utils::EventLogger::instance().log(
                utils::EventCategory::VOICE, user_id.value,
                "reconcile_untrusted_remote_participant", 0,
                "channel=" + channel.value + " reason=" + std::string(reason) +
                    " app_session_id=" + std::to_string(metadata.app_session_id) +
                    " has_intent_nonce=" + (metadata.intent_nonce.empty() ? "0" : "1") +
                    " all_queries_succeeded=" + (all_queries_succeeded ? "1" : "0") +
                    " removed=" + (removed ? "1" : "0"));
            continue;
        }

        livekit::webhook::LiveKitEvent synthetic_event;
        synthetic_event.type = livekit::webhook::LiveKitEventType::PARTICIPANT_JOINED;
        synthetic_event.raw_event_name = "participant_joined(reconcile)";
        synthetic_event.channel_id = channel;
        synthetic_event.user_id = user_id;
        synthetic_event.participant_sid = participant.sid;
        synthetic_event.participant_metadata = participant.metadata;
        synthetic_event.app_session_id = metadata.app_session_id;
        synthetic_event.intent_nonce = metadata.intent_nonce;
        synthetic_event.node_id = metadata.node_id;  // informational only
        synthetic_event.timestamp_ms =
            static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                      std::chrono::system_clock::now().time_since_epoch())
                                      .count());

        // Additive recovery (safe direction): LiveKit shows a correlated participant we don't
        // track yet (e.g. a missed join webhook) → adopt them. The harmless false-positive.
        handle_participant_joined(synthetic_event);
    }
}

void VoiceService::handle_participant_joined(const livekit::webhook::LiveKitEvent& event) {
    reconciler_.clear_channel_remote_missing_confirmation(event.channel_id);
    reconciler_.clear_participant_remote_missing_confirmation(event.channel_id, event.user_id);

    const auto pending_opt = pending_intents_.read(event.user_id);

    PendingJoinIntent intent;
    bool has_correlated_intent = false;

    if (pending_opt.has_value()) {
        intent = *pending_opt;
        if (event.channel_id == intent.to_channel) {
            if (event.app_session_id != 0 || !event.intent_nonce.empty()) {
                has_correlated_intent = event.app_session_id == intent.session_id &&
                                        !intent.intent_nonce.empty() &&
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
        // The pending intent may already have been consumed by a reconcile that
        // adopted this exact join moments earlier (synthetic "participant_joined
        // (reconcile)"), which clears the intent. The real webhook then arrives with
        // a session id but no matching intent. If this session already owns voice in
        // this channel, the event is a confirming duplicate, not a mismatch — never
        // kick the user we just admitted.
        const auto current_channel = sessions_.user_channel(event.user_id);
        const auto current_owner = sessions_.user_session(event.user_id);
        const bool already_owner_here =
            event.app_session_id != 0 && current_channel.has_value() &&
            *current_channel == event.channel_id && current_owner.has_value() &&
            *current_owner == static_cast<SessionId>(event.app_session_id);
        if (already_owner_here) {
            utils::EventLogger::instance().log(
                utils::EventCategory::VOICE, event.user_id.value, "join_already_owner", 0,
                "channel=" + event.channel_id.value +
                    " session=" + std::to_string(event.app_session_id));
            return;
        }

        utils::EventLogger::instance().log(
            utils::EventCategory::VOICE, event.user_id.value, "join_intent_mismatch", 0,
            "event_channel=" + event.channel_id.value +
                " event_session=" + std::to_string(event.app_session_id));
        reconciler_.request(event.channel_id, "join_intent_mismatch");
        (void)kick_user(event.channel_id, event.user_id);
        return;
    }

    const bool first_join_in_channel =
        sessions_.join(event.channel_id, event.user_id, intent.session_id);
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

    const std::string_view join_source =
        event.raw_event_name == "participant_joined(reconcile)" ? "reconcile" : "webhook";
    utils::EventLogger::instance().voice_join(event.user_id.value, event.channel_id.value,
                                              join_source);

    if (!resume_registry_.read(event.user_id).has_value()) {
        (void)resume_registry_.rotate(event.user_id);
    }

    if (auto channel = hub_service_.getChannel(event.channel_id)) {
        if (first_join_in_channel) {
            publisher_.publish_voice_snapshot(channel->hub_id, event.channel_id,
                                   read_channel_started_at_unix(event.channel_id));
        }
        publisher_.publish_voice_participant_upsert(channel->hub_id, event.channel_id, event.user_id,
                                         final_muted, final_deafened);
    }

    publisher_.publish_self_status(event.user_id, true, intent.session_id, event.channel_id);

    // Re-sync the current key to the just-joined member. Their token carried the key as
    // of join_voice time; if another join/leave rotated it in the meantime (concurrent
    // membership change), this delivers the live key so everyone converges.
    resync_voice_key_for_user(event.user_id);

    if (pending_opt.has_value()) {
        if (intent.has_from_channel && intent.from_channel != intent.to_channel) {
            intent.new_join_seen = true;
            if (intent.old_leave_seen) {
                clear_pending_join_intent(event.user_id);
            } else {
                (void)pending_intents_.update(event.user_id, intent);
            }
        } else {
            clear_pending_join_intent(event.user_id);
        }
    }
}

void VoiceService::handle_participant_left(const livekit::webhook::LiveKitEvent& event) {
    const auto pending_opt = pending_intents_.read(event.user_id);

    const auto current_channel = sessions_.user_channel(event.user_id);
    const auto current_owner = sessions_.user_session(event.user_id);

    const bool matches_owner_session =
        event.app_session_id == 0 ||
        (current_owner.has_value() && *current_owner == event.app_session_id);

    const bool nonce_mismatch_to_pending =
        pending_opt.has_value() && pending_opt->to_channel == event.channel_id &&
        !pending_opt->intent_nonce.empty() && !event.intent_nonce.empty() &&
        event.intent_nonce != pending_opt->intent_nonce;

    const bool leaving_current_voice = current_channel.has_value() &&
                                       *current_channel == event.channel_id &&
                                       matches_owner_session && !nonce_mismatch_to_pending;

    const bool old_leave_for_switch = pending_opt.has_value() && pending_opt->has_from_channel &&
                                      pending_opt->from_channel == event.channel_id &&
                                      pending_opt->from_channel != pending_opt->to_channel;

    if (leaving_current_voice || old_leave_for_switch) {
        reconciler_.clear_participant_remote_missing_confirmation(event.channel_id, event.user_id);
    }

    bool became_empty = false;
    if (leaving_current_voice || old_leave_for_switch) {
        became_empty = sessions_.leave(event.channel_id, event.user_id);
    }

    if (leaving_current_voice) {
        utils::EventLogger::instance().voice_leave(event.user_id.value, event.channel_id.value);

        if (auto channel = hub_service_.getChannel(event.channel_id)) {
            publisher_.publish_voice_participant_remove(channel->hub_id, event.channel_id, event.user_id);
        }

        publisher_.publish_self_status(event.user_id, false, std::nullopt, std::nullopt);

        if (!old_leave_for_switch) {
            resume_registry_.clear(event.user_id);
        }

        if (became_empty) {
            clear_channel_started_at_unix(event.channel_id);
            on_channel_empty(event.channel_id);
        } else {
            // Others remain → rotate so the departed member cannot decrypt future audio.
            channel_keys_.rotate_and_broadcast(event.channel_id, "participant_left");
        }

        if (old_leave_for_switch) {
            auto intent = *pending_opt;
            intent.old_leave_seen = true;
            if (intent.new_join_seen) {
                clear_pending_join_intent(event.user_id);
            } else {
                (void)pending_intents_.update(event.user_id, intent);
            }
        }
        return;
    }

    if (old_leave_for_switch) {
        if (auto channel = hub_service_.getChannel(event.channel_id)) {
            publisher_.publish_voice_participant_remove(channel->hub_id, event.channel_id, event.user_id);
        }

        if (sessions_.is_empty(event.channel_id)) {
            clear_channel_started_at_unix(event.channel_id);
            on_channel_empty(event.channel_id);
        } else {
            // The switcher left this (from) channel but others remain → rotate so they
            // cannot decrypt the channel's future audio.
            channel_keys_.rotate_and_broadcast(event.channel_id, "participant_switch_away");
        }

        auto intent = *pending_opt;
        intent.old_leave_seen = true;
        if (intent.new_join_seen) {
            clear_pending_join_intent(event.user_id);
        } else {
            (void)pending_intents_.update(event.user_id, intent);
        }
        return;
    }

    utils::EventLogger::instance().log(
        utils::EventCategory::VOICE, event.user_id.value, "ignored_participant_left", 0,
        "channel=" + event.channel_id.value +
            " event_session=" + std::to_string(event.app_session_id));
    reconciler_.request(event.channel_id, "ignored_participant_left");
}

void VoiceService::recover_from_restart() {
    for (const auto& channel : sessions_.active_channels()) {
        sessions_.clear_channel(channel);
        nodes_.clear_room(channel);
    }
    {
        std::lock_guard lock(channel_started_mutex_);
        channel_started_at_unix_.clear();
    }
    {
        std::lock_guard lock(event_order_mutex_);
        user_last_event_ts_ms_.clear();
        channel_last_room_event_ts_ms_.clear();
    }
    resume_registry_.clear_all();

    std::unordered_set<ChannelId> recovered_rooms;
    std::unordered_map<UserId, ChannelId> recovered_users;
    std::size_t recovered_participant_count = 0;
    const uint64_t recovered_started_at = unix_now_seconds();

    const auto nodes = nodes_.list_nodes();

    for (const auto& node : nodes) {
        livekit::cli::LivekitClient client(node.private_host, token_service_);

        std::vector<livekit::cli::RoomInfo> rooms;
        try {
            rooms = client.ListRooms();
        } catch (const std::exception& ex) {
            utils::EventLogger::instance().log(utils::EventCategory::VOICE, "",
                                               "recovery_list_rooms_failed", 0,
                                               "node=" + node.node_id + " error=" + ex.what());
            continue;
        }

        for (const auto& room_info : rooms) {
            const auto& room = room_info.room;
            if (recovered_rooms.insert(room).second) {
                nodes_.bind_room(room, node.node_id);
                nodes_.increment_room(room, node.node_id);
                set_channel_started_at_unix(room, recovered_started_at);
            }

            std::vector<livekit::cli::ParticipantInfo> participants;
            try {
                participants = client.ListParticipants(room);
            } catch (const std::exception& ex) {
                utils::EventLogger::instance().log(utils::EventCategory::VOICE, "",
                                                   "recovery_list_participants_failed", 0,
                                                   "room=" + room.value + " error=" + ex.what());
                continue;
            }

            std::unordered_map<UserId, livekit::cli::ParticipantInfo> participants_by_user;
            for (const auto& participant : participants) {
                if (participant.identity.value.empty()) continue;
                participants_by_user.emplace(participant.identity, participant);
            }
            const auto metadata_node_id =
                resolve_participant_node_assignment(room, participants_by_user, "recovery");
            if (metadata_node_id.has_value()) {
                const auto previous_node = nodes_.get_room_node(room);
                if (!previous_node || previous_node->node_id != *metadata_node_id) {
                    utils::EventLogger::instance().log(
                        utils::EventCategory::VOICE, "", "recovery_room_assignment_repaired", 0,
                        "channel=" + room.value + " previous_node=" +
                            (previous_node ? previous_node->node_id : std::string("unknown")) +
                            " assigned_node=" + *metadata_node_id + " source=participant_metadata");
                }
                nodes_.bind_room(room, *metadata_node_id);
                nodes_.increment_room(room, *metadata_node_id);
            }

            if (!participants.empty()) {
                if (!channel_keys_.restore_key_for_recovery(room)) {
                    continue;  // no valid stored key → force-rekeyed; skip registering
                }
            } else {
                channel_keys_.clear_rekey_guard(room);
            }

            for (const auto& p : participants) {
                if (p.identity.value.empty()) {
                    continue;
                }

                const auto [it, inserted] = recovered_users.emplace(p.identity, room);
                if (!inserted) {
                    if (it->second != room) {
                        utils::EventLogger::instance().log(
                            utils::EventCategory::VOICE, p.identity.value,
                            "recovery_user_multiple_channels", 0,
                            "first_room=" + it->second.value + " second_room=" + room.value);
                    }
                    continue;
                }

                const auto metadata = parse_participant_metadata(p.metadata);
                const SessionId recovered_session =
                    metadata.app_session_id != 0 ? static_cast<SessionId>(metadata.app_session_id)
                                                 : next_recovery_session_id();

                const bool first_join_in_channel =
                    sessions_.join(room, p.identity, recovered_session);
                (void)sessions_.set_muted(p.identity, false);
                (void)sessions_.set_deafened(p.identity, false);

                if (first_join_in_channel) {
                    set_channel_started_at_unix(room, recovered_started_at);
                }

                const std::string effective_node_id =
                    !metadata.node_id.empty() ? metadata.node_id : node.node_id;
                if (!effective_node_id.empty()) {
                    nodes_.increment_user(effective_node_id);
                }

                if (!resume_registry_.read(p.identity).has_value()) {
                    if (!resume_registry_.load_from_storage(p.identity).has_value()) {
                        (void)resume_registry_.rotate(p.identity);
                    }
                }

                recovered_participant_count++;
            }
        }
    }

    utils::EventLogger::instance().log(
        utils::EventCategory::VOICE, "", "recovery_completed", 0,
        "rooms=" + std::to_string(recovered_rooms.size()) +
            " participants=" + std::to_string(recovered_participant_count));
}

void VoiceService::on_livekit_event(const livekit::webhook::LiveKitEvent& event) {
    using Type = livekit::webhook::LiveKitEventType;

    if (!mark_webhook_event_seen(event.event_id)) {
        utils::EventLogger::instance().log(utils::EventCategory::VOICE, event.user_id.value,
                                           "duplicate_webhook_event", 0,
                                           "event_id=" + event.event_id);
        return;
    }

    if ((event.type == Type::PARTICIPANT_JOINED || event.type == Type::PARTICIPANT_LEFT ||
         event.type == Type::PARTICIPANT_CONNECTION_ABORTED) &&
        is_stale_participant_event(event)) {
        utils::EventLogger::instance().log(
            utils::EventCategory::VOICE, event.user_id.value, "stale_participant_event", 0,
            "event_id=" + event.event_id + " raw_event=" + event.raw_event_name + " channel=" +
                event.channel_id.value + " timestamp_ms=" + std::to_string(event.timestamp_ms));
        reconciler_.request(event.channel_id, "stale_participant_event");
        return;
    }

    if ((event.type == Type::ROOM_STARTED || event.type == Type::ROOM_FINISHED) &&
        is_stale_room_event(event)) {
        utils::EventLogger::instance().log(
            utils::EventCategory::VOICE, "", "stale_room_event", 0,
            "event_id=" + event.event_id + " raw_event=" + event.raw_event_name + " channel=" +
                event.channel_id.value + " timestamp_ms=" + std::to_string(event.timestamp_ms));
        reconciler_.request(event.channel_id, "stale_room_event");
        return;
    }

    auto bound_node = nodes_.get_room_node(event.channel_id);
    const std::string current_node_id = bound_node ? bound_node->node_id : "";

    switch (event.type) {
        case Type::ROOM_STARTED:
            if (!current_node_id.empty()) nodes_.increment_room(event.channel_id, current_node_id);
            break;

        case Type::ROOM_FINISHED:
            nodes_.decrement_room(event.channel_id);

            if (consume_channel_takeover_guard(event.channel_id)) {
                utils::EventLogger::instance().log(utils::EventCategory::VOICE, "",
                                                   "room_finished_guarded", 0,
                                                   "channel=" + event.channel_id.value);
                break;
            }

            utils::EventLogger::instance().log(utils::EventCategory::VOICE, "",
                                               "room_finished_requires_reconcile", 0,
                                               "channel=" + event.channel_id.value);
            reconciler_.reset_remote_missing_confirmations(event.channel_id);
            reconciler_.request(event.channel_id, "room_finished");
            break;

        case Type::PARTICIPANT_JOINED: {
            const auto before_channel = sessions_.user_channel(event.user_id);
            std::string effective_node_id = current_node_id;
            if (!event.node_id.empty() && current_node_id.empty()) {
                nodes_.bind_room(event.channel_id, event.node_id);
                effective_node_id = event.node_id;
            } else if (!event.node_id.empty() && event.node_id != current_node_id) {
                utils::EventLogger::instance().log(utils::EventCategory::VOICE, event.user_id.value,
                                                   "participant_joined_node_mismatch", 0,
                                                   "event_node=" + event.node_id +
                                                       " current_node=" + current_node_id +
                                                       " channel=" + event.channel_id.value);
            }

            handle_participant_joined(event);

            const auto after_channel = sessions_.user_channel(event.user_id);
            const bool entered_channel =
                after_channel.has_value() && *after_channel == event.channel_id &&
                (!before_channel.has_value() || *before_channel != event.channel_id);
            if (entered_channel) {
                if (!effective_node_id.empty()) {
                    nodes_.increment_user(effective_node_id);
                }
            }
            break;
        }

        case Type::PARTICIPANT_LEFT:
        case Type::PARTICIPANT_CONNECTION_ABORTED: {
            const bool node_mismatch = !event.node_id.empty() && !current_node_id.empty() &&
                                       event.node_id != current_node_id;
            if (node_mismatch) {
                utils::EventLogger::instance().log(utils::EventCategory::VOICE, event.user_id.value,
                                                   "participant_left_node_mismatch", 0,
                                                   "event_node=" + event.node_id +
                                                       " current_node=" + current_node_id +
                                                       " channel=" + event.channel_id.value);
            }

            const auto before_channel = sessions_.user_channel(event.user_id);
            handle_participant_left(event);

            const auto after_channel = sessions_.user_channel(event.user_id);
            const bool left_channel =
                before_channel.has_value() && *before_channel == event.channel_id &&
                (!after_channel.has_value() || *after_channel != event.channel_id);
            if (left_channel) {
                if (!event.node_id.empty()) {
                    nodes_.decrement_user(event.node_id);
                } else if (!current_node_id.empty()) {
                    nodes_.decrement_user(current_node_id);
                }
            }
            if (node_mismatch) {
                reconciler_.request(event.channel_id, "participant_left_node_mismatch");
            }
            break;
        }

        case Type::TRACK_PUBLISHED:
        case Type::TRACK_UNPUBLISHED:
        case Type::EGRESS_STARTED:
        case Type::EGRESS_UPDATED:
        case Type::EGRESS_ENDED:
        case Type::INGRESS_STARTED:
        case Type::INGRESS_ENDED: {
            utils::EventLogger::instance().log(
                utils::EventCategory::VOICE, event.user_id.value, "livekit_telemetry_event", 0,
                "event_id=" + event.event_id + " raw_event=" + event.raw_event_name +
                    " channel=" + event.channel_id.value + " track_sid=" + event.track_sid +
                    " egress_id=" + event.egress_id + " ingress_id=" + event.ingress_id);
            break;
        }

        case Type::UNKNOWN:
        default:
            utils::EventLogger::instance().log(utils::EventCategory::VOICE, event.user_id.value,
                                               "unknown_or_unsupported_webhook_event", 0,
                                               "event_id=" + event.event_id +
                                                   " raw_event=" + event.raw_event_name +
                                                   " channel=" + event.channel_id.value);
            break;
    }
}

void VoiceService::on_session_destroyed(const UserId& user) {
    const auto voice_channel = sessions_.user_channel(user);
    if (!voice_channel.has_value()) return;
    reconciler_.request(*voice_channel, "session_destroyed");
}

std::string VoiceService::generate_intent_nonce() const { return generate_nonce_hex(); }

std::uint64_t VoiceService::active_voice_user_count() const {
    return sessions_.active_voice_user_count();
}

std::optional<std::string> VoiceService::current_resume_id(const UserId& user) const {
    return resume_registry_.read(user);
}

std::optional<VoiceService::ResumeTransferResult> VoiceService::try_resume_voice_ownership(
    const UserId& user, const HubId& hub, const ChannelId& channel, std::string_view resume_id,
    SessionId next_owner_session) {
    if (resume_id.empty()) return std::nullopt;

    const auto active_channel = sessions_.user_channel(user);
    if (!active_channel.has_value() || *active_channel != channel) {
        return std::nullopt;
    }

    const auto channel_info = hub_service_.getChannel(*active_channel);
    if (!channel_info || channel_info->hub_id != hub) {
        return std::nullopt;
    }

    const auto owner_session = sessions_.user_session(user);
    if (!owner_session.has_value() || *owner_session == next_owner_session) {
        return std::nullopt;
    }

    const auto current_resume = resume_registry_.read(user);
    if (!current_resume.has_value() || *current_resume != resume_id) {
        return std::nullopt;
    }

    if (!sessions_.transfer_owner_session(user, *owner_session, next_owner_session)) {
        return std::nullopt;
    }

    ResumeTransferResult result;
    result.previous_owner_session = *owner_session;
    result.channel = *active_channel;
    result.resume_id = resume_registry_.rotate(user);
    return result;
}

void VoiceService::resync_voice_key_for_user(const UserId& user) {
    channel_keys_.resync_to_user(user);
}

std::string VoiceService::webhook_seen_key(const std::string& event_id) {
    return "voice:webhook_seen:" + event_id;
}

}  // namespace app::services::voice
