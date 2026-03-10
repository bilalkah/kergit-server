#include "app/services/voice/VoiceService.h"

#include "livekit/cli/LivekitClient.h"
#include "utils/EnvLoader.h"
#include "utils/EventLogger.h"

namespace app::services::voice {

VoiceService::VoiceService(std::string api_key, std::string api_secret,
                           VoiceStateRepository& voice_state_repo)
    : voice_state_repo_(voice_state_repo),
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

sercom::protocol::event::VoiceTokenIssued VoiceService::join_voice(const ChannelId& channel,
                                                                   const UserId& user) {
    sercom::protocol::event::VoiceTokenIssued response;

    auto node = nodes_.get_room_node(channel);
    if (!node) {
        node = nodes_.pick_node();
        if (node) nodes_.bind_room(channel, node->node_id);
    }

    if (!node) return response;

    livekit::LiveKitTokenService::ParticipantTokenRequest req{.identity = user,
                                                              .room = channel,
                                                              .node_id = node->node_id,
                                                              .can_publish = true,
                                                              .can_subscribe = true,
                                                              .ttl = std::chrono::minutes(10)};

    const std::string token = token_service_.mint_participant_token(req);
    const std::string key = e2ee_keys_.get_or_create_key(channel);

    response.set_token(token);
    response.set_livekit_url(node->public_host);
    response.set_expires_in(600);
    response.set_e2ee_key(key);

    return response;
}

void VoiceService::on_channel_empty(const ChannelId& channel) {
    try {
        voice_state_repo_.remove_channel(channel);
    } catch (...) {
    }
    e2ee_keys_.clear_key(channel);
    nodes_.clear_room(channel);
}

bool VoiceService::kick_user(const ChannelId& channel, const UserId& target) {
    auto node = nodes_.get_room_node(channel);
    if (!node) return false;

    livekit::cli::LivekitClient client(node->private_host, token_service_);
    return client.RemoveParticipant(channel, target);
}

void VoiceService::mark_takeover(const UserId& user) {
    std::lock_guard lock(takeover_guard_mutex_);
    takeover_left_guard_[user] = std::chrono::steady_clock::now() + kTakeoverGuardTtl;
}

void VoiceService::mark_channel_takeover(const ChannelId& channel) {
    std::lock_guard lock(takeover_guard_mutex_);
    channel_takeover_guard_[channel] = std::chrono::steady_clock::now() + kTakeoverGuardTtl;
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

bool VoiceService::consume_takeover_left_guard(const UserId& user) {
    std::lock_guard lock(takeover_guard_mutex_);
    auto it = takeover_left_guard_.find(user);
    if (it == takeover_left_guard_.end()) return false;

    if (it->second < std::chrono::steady_clock::now()) {
        takeover_left_guard_.erase(it);
        return false;
    }

    takeover_left_guard_.erase(it);
    return true;
}

ParticipantState VoiceService::consume_pending_preferences(const UserId& user) {
    std::lock_guard lock(takeover_guard_mutex_);
    auto it = pending_preferences_.find(user);
    if (it == pending_preferences_.end()) return {};
    auto prefs = it->second;
    pending_preferences_.erase(it);
    return prefs;
}

void VoiceService::persist_voice_join(const UserId& user, const ChannelId& channel) {
    // Apply any cached recovery preferences to in-memory state.
    auto prefs = consume_pending_preferences(user);
    if (prefs.muted) sessions_.set_muted(user, true);
    if (prefs.deafened) sessions_.set_deafened(user, true);

    try {
        voice_state_repo_.upsert(user, channel, prefs.muted, prefs.deafened);
    } catch (const std::exception& ex) {
        utils::EventLogger::instance().log(utils::EventCategory::VOICE, user.value,
                                           "persist_join_failed", 0,
                                           "channel=" + channel.value + " error=" + ex.what());
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

void VoiceService::persist_mute_state(const UserId& user, bool muted, bool deafened) {
    try {
        voice_state_repo_.update_mute_state(user, muted, deafened);
    } catch (const std::exception& ex) {
        utils::EventLogger::instance().log(
            utils::EventCategory::VOICE, user.value, "persist_mute_failed", 0,
            "muted=" + std::to_string(muted) + " deafened=" + std::to_string(deafened) +
                " error=" + ex.what());
    }
}

void VoiceService::recover_from_restart() {
    // Load mute/deafen preferences from DB before clearing.
    try {
        auto rows = voice_state_repo_.load_all();
        {
            std::lock_guard lock(takeover_guard_mutex_);
            for (const auto& row : rows) {
                if (row.muted || row.deafened) {
                    pending_preferences_[row.user_id] = {row.muted, row.deafened};
                }
            }
        }
        utils::EventLogger::instance().log(
            utils::EventCategory::VOICE, "", "recovery_loaded_prefs", 0,
            "rows=" + std::to_string(rows.size()) +
                " prefs_cached=" + std::to_string(pending_preferences_.size()));

        voice_state_repo_.clear_all();
    } catch (const std::exception& ex) {
        utils::EventLogger::instance().log(utils::EventCategory::VOICE, "", "recovery_db_failed", 0,
                                           std::string("error=") + ex.what());
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

        if (rooms.empty()) continue;

        utils::EventLogger::instance().log(
            utils::EventCategory::VOICE, "", "recovery_found_rooms", 0,
            "node=" + node.node_id + " rooms=" + std::to_string(rooms.size()));

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

            utils::EventLogger::instance().log(utils::EventCategory::VOICE, "",
                                               "recovery_kicking_participants", 0,
                                               "node=" + node.node_id + " room=" + room.value +
                                                   " count=" + std::to_string(participants.size()));

            for (const auto& p : participants) {
                client.RemoveParticipant(room, p.identity);
            }
        }
    }
}

void VoiceService::on_livekit_event(const livekit::webhook::LiveKitEvent& event) {
    using Type = livekit::webhook::LiveKitEventType;

    auto node = nodes_.get_room_node(event.channel_id);
    const std::string node_id = node ? node->node_id : "";

    switch (event.type) {
        case Type::ROOM_STARTED:
            if (!node_id.empty()) nodes_.increment_room(node_id);
            break;

        case Type::ROOM_FINISHED:
            if (!node_id.empty()) nodes_.decrement_room(node_id);

            // If a takeover is in progress, the new session hasn't connected yet.
            // Preserve server-side state so the new session can join cleanly.
            if (consume_channel_takeover_guard(event.channel_id)) {
                utils::EventLogger::instance().log(utils::EventCategory::VOICE, "",
                                                   "room_finished_guarded", 0,
                                                   "channel=" + event.channel_id.value +
                                                       " (takeover in progress, preserving state)");
                break;
            }

            {
                const auto removed_users = sessions_.clear_channel(event.channel_id);
                std::lock_guard lock(takeover_guard_mutex_);
                for (const auto& user_id : removed_users) {
                    takeover_left_guard_.erase(user_id);
                }
            }
            try {
                voice_state_repo_.remove_channel(event.channel_id);
            } catch (...) {
            }
            nodes_.clear_room(event.channel_id);
            e2ee_keys_.clear_key(event.channel_id);
            break;

        case Type::PARTICIPANT_JOINED:
            if (!node_id.empty()) nodes_.increment_user(node_id);
            // Session state is set by the command handler (join_voice flow),
            // not here — webhook just confirms what we already staged.
            utils::EventLogger::instance().voice_join(event.user_id.value, event.channel_id.value);
            break;

        case Type::PARTICIPANT_LEFT: {
            if (!node_id.empty()) nodes_.decrement_user(node_id);

            // The old owner session may disconnect after takeover; ignore that stale leave.
            if (consume_takeover_left_guard(event.user_id)) {
                break;
            }

            bool became_empty = sessions_.leave(event.channel_id, event.user_id);
            utils::EventLogger::instance().voice_leave(event.user_id.value, event.channel_id.value);
            try {
                voice_state_repo_.remove(event.user_id);
            } catch (...) {
            }
            if (became_empty) {
                on_channel_empty(event.channel_id);
            }
            break;
        }

        case Type::UNKNOWN:
        default:
            break;
    }
}

}  // namespace app::services::voice
