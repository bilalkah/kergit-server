#include "app/services/hub/HubNotifier.h"

#include "app/commands/utils.h"
#include "proto/envelope.pb.h"
#include "proto/event/channel.pb.h"
#include "proto/event/hub.pb.h"
#include "proto/event/presence.pb.h"

namespace app::services {

namespace {
inline sercom::protocol::event::PresenceEvent make_presence_changed(std::string_view hub_id,
                                                                    std::string_view user_id,
                                                                    bool is_online) {
    sercom::protocol::event::PresenceEvent presence;
    auto* payload = presence.mutable_presence_changed();
    payload->set_hub_id(hub_id.data(), hub_id.size());
    payload->set_user_id(user_id.data(), user_id.size());
    payload->set_is_online(is_online);
    return presence;
}

}  // namespace

std::string HubNotifier::hubUpdated(const HubId& hubId, const std::string& name,
                                    const std::string& avatar_seed) {
    sercom::protocol::event::HubUpdated updated;
    updated.set_hub_id(hubId.value);
    updated.set_action(sercom::protocol::event::HUB_ACTION_UPDATED);
    auto* hub = updated.mutable_hub();
    hub->set_id(hubId.value);
    hub->set_name(name);
    hub->mutable_metadata()->set_avatar_seed(avatar_seed);
    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(sercom::protocol::Envelope::HUB_UPDATED);
    updated.SerializeToString(env.mutable_payload());
    return env.SerializeAsString();
}

std::string HubNotifier::hubRemoved(const HubId& hubId) {
    sercom::protocol::event::HubUpdated updated;
    updated.set_hub_id(hubId.value);
    updated.set_action(sercom::protocol::event::HUB_ACTION_REMOVED);
    auto* hub = updated.mutable_hub();
    hub->set_id(hubId.value);
    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(sercom::protocol::Envelope::HUB_UPDATED);
    updated.SerializeToString(env.mutable_payload());
    return env.SerializeAsString();
}

std::string HubNotifier::memberJoined(const HubId& hubId, const UserId& userId, Role role,
                                      const std::string& username, const std::string& avatar_seed,
                                      bool is_online) {
    sercom::protocol::event::HubMemberJoined joined;
    auto* member = joined.mutable_member();
    member->set_hub_id(hubId.value);
    member->set_user_id(userId.value);
    member->set_is_online(is_online);
    member->set_role(to_proto_hub_role(role));

    auto* user = joined.mutable_user();
    user->set_id(userId.value);
    user->mutable_metadata()->set_username(username);
    user->mutable_metadata()->set_avatar_seed(avatar_seed);

    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(sercom::protocol::Envelope::HUB_MEMBER_JOINED);
    joined.SerializeToString(env.mutable_payload());
    return env.SerializeAsString();
}

std::string HubNotifier::memberLeft(const HubId& hubId, const UserId& userId) {
    sercom::protocol::event::HubMemberLeft left;
    left.set_hub_id(hubId.value);
    left.set_user_id(userId.value);
    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(sercom::protocol::Envelope::HUB_MEMBER_LEFT);
    left.SerializeToString(env.mutable_payload());
    return env.SerializeAsString();
}

std::string HubNotifier::memberOnline(const HubId& hubId, const UserId& userId) {
    auto pe = make_presence_changed(hubId.value, userId.value, true);
    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(sercom::protocol::Envelope::PRESENCE);
    pe.SerializeToString(env.mutable_payload());
    return env.SerializeAsString();
}

std::string HubNotifier::memberOffline(const HubId& hubId, const UserId& userId) {
    auto pe = make_presence_changed(hubId.value, userId.value, false);
    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(sercom::protocol::Envelope::PRESENCE);
    pe.SerializeToString(env.mutable_payload());
    return env.SerializeAsString();
}

std::string HubNotifier::channelCreated(const HubId& hubId, const Channel& channel) {
    sercom::protocol::event::ChannelUpdated updated;
    updated.set_hub_id(hubId.value);
    updated.set_action(sercom::protocol::event::CHANNEL_ACTION_CREATED);
    auto* out_channel = updated.mutable_channel();
    out_channel->set_id(channel.id.value);
    out_channel->set_hub_id(hubId.value);
    out_channel->set_type(to_proto_channel_type(channel.type));
    out_channel->mutable_metadata()->set_name(channel.name);
    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(sercom::protocol::Envelope::CHANNEL_UPDATED);
    updated.SerializeToString(env.mutable_payload());
    return env.SerializeAsString();
}

std::string HubNotifier::channelUpdated(const HubId& hubId, const Channel& channel) {
    sercom::protocol::event::ChannelUpdated updated;
    updated.set_hub_id(hubId.value);
    updated.set_action(sercom::protocol::event::CHANNEL_ACTION_UPDATED);
    auto* out_channel = updated.mutable_channel();
    out_channel->set_id(channel.id.value);
    out_channel->set_hub_id(hubId.value);
    out_channel->set_type(to_proto_channel_type(channel.type));
    out_channel->mutable_metadata()->set_name(channel.name);
    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(sercom::protocol::Envelope::CHANNEL_UPDATED);
    updated.SerializeToString(env.mutable_payload());
    return env.SerializeAsString();
}

std::string HubNotifier::channelRemoved(const HubId& hubId, const ChannelId& channelId) {
    sercom::protocol::event::ChannelUpdated updated;
    updated.set_hub_id(hubId.value);
    updated.set_action(sercom::protocol::event::CHANNEL_ACTION_REMOVED);
    auto* out_channel = updated.mutable_channel();
    out_channel->set_id(channelId.value);
    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(sercom::protocol::Envelope::CHANNEL_UPDATED);
    updated.SerializeToString(env.mutable_payload());
    return env.SerializeAsString();
}
}  // namespace app::services
