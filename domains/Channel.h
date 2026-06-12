#ifndef DOMAINS_CHANNEL_H
#define DOMAINS_CHANNEL_H

#include "domains/ids/Ids.h"

#include <string>

enum class ChannelType { CHAT, VOICE };

struct Channel {
    Channel() {}
    Channel(std::string channel_name, ChannelId channel_id, HubId hub_id, ChannelType channel_type)
        : name(std::move(channel_name)),
          id(std::move(channel_id)),
          hub_id(std::move(hub_id)),
          type(channel_type) {}

    std::string name{""};
    ChannelId id{""};
    HubId hub_id{""};
    ChannelType type{ChannelType::CHAT};
};

#endif  // DOMAINS_CHANNEL_H
