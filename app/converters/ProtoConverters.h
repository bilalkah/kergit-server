#ifndef APP_CONVERTERS_PROTOCONVERTERS_H
#define APP_CONVERTERS_PROTOCONVERTERS_H

#include "domains/Channel.h"
#include "domains/Hub.h"
#include "proto/domain/channel.pb.h"
#include "proto/domain/hub.pb.h"

namespace app::converters {

inline sercom::protocol::domain::ChannelType to_proto_channel_type(ChannelType type) {
    using ProtoChannelType = sercom::protocol::domain::ChannelType;
    switch (type) {
        case ChannelType::VOICE:
            return ProtoChannelType::CHANNEL_TYPE_VOICE;
        case ChannelType::CHAT:
        default:
            return ProtoChannelType::CHANNEL_TYPE_TEXT;
    }
}

inline ChannelType from_proto_channel_type(sercom::protocol::domain::ChannelType type) {
    using ProtoChannelType = sercom::protocol::domain::ChannelType;
    switch (type) {
        case ProtoChannelType::CHANNEL_TYPE_VOICE:
            return ChannelType::VOICE;
        case ProtoChannelType::CHANNEL_TYPE_TEXT:
        case ProtoChannelType::CHANNEL_TYPE_UNSPECIFIED:
        default:
            return ChannelType::CHAT;
    }
}

inline sercom::protocol::domain::HubRole to_proto_hub_role(Role role) {
    using ProtoHubRole = sercom::protocol::domain::HubRole;
    switch (role) {
        case Role::OWNER:
            return ProtoHubRole::HUB_ROLE_OWNER;
        case Role::ADMIN:
            return ProtoHubRole::HUB_ROLE_ADMIN;
        case Role::USER:
        default:
            return ProtoHubRole::HUB_ROLE_MEMBER;
    }
}

inline Role from_proto_hub_role(sercom::protocol::domain::HubRole role) {
    using ProtoHubRole = sercom::protocol::domain::HubRole;
    switch (role) {
        case ProtoHubRole::HUB_ROLE_OWNER:
            return Role::OWNER;
        case ProtoHubRole::HUB_ROLE_ADMIN:
            return Role::ADMIN;
        case ProtoHubRole::HUB_ROLE_MEMBER:
        case ProtoHubRole::HUB_ROLE_UNSPECIFIED:
        default:
            return Role::USER;
    }
}

}  // namespace app::converters

#endif  // APP_CONVERTERS_PROTOCONVERTERS_H
