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
            return ProtoChannelType::ChannelType_VOICE;
        case ChannelType::CHAT:
        default:
            return ProtoChannelType::ChannelType_TEXT;
    }
}

inline ChannelType from_proto_channel_type(sercom::protocol::domain::ChannelType type) {
    using ProtoChannelType = sercom::protocol::domain::ChannelType;
    switch (type) {
        case ProtoChannelType::ChannelType_VOICE:
            return ChannelType::VOICE;
        case ProtoChannelType::ChannelType_TEXT:
        case ProtoChannelType::ChannelType_UNSPECIFIED:
        default:
            return ChannelType::CHAT;
    }
}

inline sercom::protocol::domain::HubRole to_proto_hub_role(Role role) {
    using ProtoHubRole = sercom::protocol::domain::HubRole;
    switch (role) {
        case Role::OWNER:
            return ProtoHubRole::HubRole_OWNER;
        case Role::ADMIN:
            return ProtoHubRole::HubRole_ADMIN;
        case Role::USER:
        default:
            return ProtoHubRole::HubRole_MEMBER;
    }
}

inline Role from_proto_hub_role(sercom::protocol::domain::HubRole role) {
    using ProtoHubRole = sercom::protocol::domain::HubRole;
    switch (role) {
        case ProtoHubRole::HubRole_OWNER:
            return Role::OWNER;
        case ProtoHubRole::HubRole_ADMIN:
            return Role::ADMIN;
        case ProtoHubRole::HubRole_MEMBER:
        case ProtoHubRole::HubRole_UNSPECIFIED:
        default:
            return Role::USER;
    }
}

}  // namespace app::converters

#endif  // APP_CONVERTERS_PROTOCONVERTERS_H
