#include "app/services/voice/VoiceService.h"

#include "livekit/cli/LivekitClient.h"
#include "utils/EventLogger.h"

namespace app::services::voice {

VoiceService::VoiceService(std::string api_key, std::string api_secret)
    : token_service_(std::move(api_key), std::move(api_secret))
{
    struct NodeConf { std::string id, priv, pub; };
    const NodeConf node_conf[] = {
        {"livekit-node1", "http://livekit-node1:7880", "https://localhost/livekit-node-1"},
        {"livekit-node2", "http://livekit-node2:7890", "https://localhost/livekit-node-2"},
    };

    for (const auto& n : node_conf) {
        nodes_.register_node({n.id, n.pub, n.priv});
    }
}

sercom::protocol::event::VoiceTokenIssued
VoiceService::join_voice(const ChannelId& channel, const UserId& user)
{
    sercom::protocol::event::VoiceTokenIssued response;

    auto node = nodes_.get_room_node(channel);
    if (!node) {
        node = nodes_.pick_node();
        if (node)
            nodes_.bind_room(channel, node->node_id);
    }

    if (!node)
        return response;

    livekit::LiveKitTokenService::ParticipantTokenRequest req{
        .identity      = user,
        .room          = channel,
        .node_id       = node->node_id,
        .can_publish   = true,
        .can_subscribe = true,
        .ttl           = std::chrono::minutes(10)
    };

    const std::string token = token_service_.mint_participant_token(req);
    const std::string key   = e2ee_keys_.get_or_create_key(channel);

    response.set_token(token);
    response.set_livekit_url(node->public_host);
    response.set_expires_in(600);
    response.set_e2ee_key(key);

    return response;
}

void VoiceService::on_channel_empty(const ChannelId& channel)
{
    e2ee_keys_.clear_key(channel);
    nodes_.clear_room(channel);
}

bool VoiceService::kick_user(const ChannelId& channel, const UserId& target)
{
    auto node = nodes_.get_room_node(channel);
    if (!node)
        return false;

    livekit::cli::LivekitClient client(node->private_host, token_service_);
    return client.RemoveParticipant(channel, target);
}

void VoiceService::mark_takeover(const UserId& user)
{
    std::lock_guard lock(takeover_guard_mutex_);
    takeover_left_guard_[user] = std::chrono::steady_clock::now() + kTakeoverGuardTtl;
}

void VoiceService::mark_channel_takeover(const ChannelId& channel)
{
    std::lock_guard lock(takeover_guard_mutex_);
    channel_takeover_guard_[channel] = std::chrono::steady_clock::now() + kTakeoverGuardTtl;
}

bool VoiceService::consume_channel_takeover_guard(const ChannelId& channel)
{
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

bool VoiceService::consume_takeover_left_guard(const UserId& user)
{
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

void VoiceService::on_livekit_event(const livekit::webhook::LiveKitEvent& event)
{
    using Type = livekit::webhook::LiveKitEventType;

    auto node = nodes_.get_room_node(event.channel_id);
    const std::string node_id = node ? node->node_id : "";

    switch (event.type) {
        case Type::ROOM_STARTED:
            if (!node_id.empty())
                nodes_.increment_room(node_id);
            break;

        case Type::ROOM_FINISHED:
            if (!node_id.empty())
                nodes_.decrement_room(node_id);

            // If a takeover is in progress, the new session hasn't connected yet.
            // Preserve server-side state so the new session can join cleanly.
            if (consume_channel_takeover_guard(event.channel_id)) {
                utils::EventLogger::instance().log(
                    utils::EventCategory::VOICE, "", "room_finished_guarded", 0,
                    "channel=" + event.channel_id.value + " (takeover in progress, preserving state)");
                break;
            }

            {
                const auto removed_users = sessions_.clear_channel(event.channel_id);
                std::lock_guard lock(takeover_guard_mutex_);
                for (const auto& user_id : removed_users) {
                    takeover_left_guard_.erase(user_id);
                }
            }
            nodes_.clear_room(event.channel_id);
            e2ee_keys_.clear_key(event.channel_id);
            break;

        case Type::PARTICIPANT_JOINED:
            if (!node_id.empty())
                nodes_.increment_user(node_id);
            // Session state is set by the command handler (join_voice flow),
            // not here — webhook just confirms what we already staged.
            utils::EventLogger::instance().voice_join(event.user_id.value, event.channel_id.value);
            break;

        case Type::PARTICIPANT_LEFT: {
            if (!node_id.empty())
                nodes_.decrement_user(node_id);

            // The old owner session may disconnect after takeover; ignore that stale leave.
            if (consume_takeover_left_guard(event.user_id)) {
                break;
            }

            bool became_empty = sessions_.leave(event.channel_id, event.user_id);
            utils::EventLogger::instance().voice_leave(event.user_id.value, event.channel_id.value);
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

} // namespace app::services::voice
