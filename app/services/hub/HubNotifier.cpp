#include "app/services/hub/HubNotifier.h"

#include "app/converters/ProtoConverters.h"
#include "app/proto_builders/EnvelopeBuilders.h"
#include "app/proto_builders/PresenceBuilders.h"
#include "proto/event/channel.pb.h"
#include "proto/event/hub.pb.h"
#include "proto/event/presence.pb.h"

namespace app::services {

std::string HubNotifier::hubRenamed(const HubId& hubId, const std::string& newName) {
    sercom::protocol::event::HubRenamed renamed;
    renamed.set_hub_id(hubId.value);
    renamed.set_name(newName);
    return proto_builders::serialize_envelope(sercom::protocol::Envelope::HUB_RENAMED, renamed);
}

std::string HubNotifier::hubDeleted(const HubId& hubId) {
    sercom::protocol::event::HubRemoved removed;
    removed.set_hub_id(hubId.value);
    return proto_builders::serialize_envelope(sercom::protocol::Envelope::HUB_REMOVED, removed);
}

std::string HubNotifier::memberJoined(const HubId& hubId, const UserId& userId, Role role,
                                      const std::string& display_name,
                                      const std::string& avatar_seed, const std::string& username,
                                      bool is_online) {
    sercom::protocol::event::HubMemberJoined joined;
    auto* member = joined.mutable_member();
    member->set_hub_id(hubId.value);
    member->set_user_id(userId.value);
    member->set_is_online(is_online);
    member->set_role(converters::to_proto_hub_role(role));
    member->set_display_name(display_name);
    member->set_avatar_seed(avatar_seed);

    auto* user = joined.mutable_user();
    user->set_id(userId.value);
    user->set_username(username);
    user->set_avatar_seed(avatar_seed);

    return proto_builders::serialize_envelope(sercom::protocol::Envelope::HUB_MEMBER_JOINED,
                                              joined);
}

std::string HubNotifier::memberLeft(const HubId& hubId, const UserId& userId) {
    sercom::protocol::event::HubMemberLeft left;
    left.set_hub_id(hubId.value);
    left.set_user_id(userId.value);
    return proto_builders::serialize_envelope(sercom::protocol::Envelope::HUB_MEMBER_LEFT, left);
}

std::string HubNotifier::memberOnline(const HubId& hubId, const UserId& userId) {
    auto pe = proto_builders::presence::make_presence_changed(hubId.value, userId.value, true);
    return proto_builders::serialize_envelope(sercom::protocol::Envelope::PRESENCE, pe);
}

std::string HubNotifier::memberOffline(const HubId& hubId, const UserId& userId) {
    auto pe = proto_builders::presence::make_presence_changed(hubId.value, userId.value, false);
    return proto_builders::serialize_envelope(sercom::protocol::Envelope::PRESENCE, pe);
}

std::string HubNotifier::channelCreated(const HubId& hubId, const Channel& channel) {
    sercom::protocol::event::ChannelCreated created;
    created.set_hub_id(hubId.value);
    auto* out_channel = created.mutable_channel();
    out_channel->set_id(channel.id.value);
    out_channel->set_name(channel.name);
    out_channel->set_type(converters::to_proto_channel_type(channel.type));
    return proto_builders::serialize_envelope(sercom::protocol::Envelope::CHANNEL_CREATED, created);
}

std::string HubNotifier::channelRenamed(const Channel& channel) {
    sercom::protocol::event::ChannelRenamed renamed;
    auto* out_channel = renamed.mutable_channel();
    out_channel->set_id(channel.id.value);
    out_channel->set_name(channel.name);
    out_channel->set_type(converters::to_proto_channel_type(channel.type));
    return proto_builders::serialize_envelope(sercom::protocol::Envelope::CHANNEL_RENAMED, renamed);
}

std::string HubNotifier::channelDeleted(const ChannelId& channelId) {
    sercom::protocol::event::ChannelRemoved removed;
    removed.set_channel_id(channelId.value);
    return proto_builders::serialize_envelope(sercom::protocol::Envelope::CHANNEL_REMOVED, removed);
}
}  // namespace app::services
