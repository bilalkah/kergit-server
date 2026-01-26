#ifndef APP_MANAGERS_SESSION_SESSIONINFO_H
#define APP_MANAGERS_SESSION_SESSIONINFO_H

#include "domains/ids/Ids.h"

#include <optional>
#include <unordered_set>
namespace app {

struct SessionInfo {
    // application-level identifiers
    std::unordered_set<HubId> snapshotted_hubs;

    // current context
    std::optional<HubId> current_hub;
    std::optional<ChannelId> current_text_channel;
    std::optional<HubId> current_voice_hub;
    std::optional<ChannelId> current_voice_channel;
    bool voice_muted = false;
    bool voice_deafened = false;

    // transport-level connection
    std::optional<GlobalConnId> main_conn;
};

}  // namespace app

#endif  // APP_MANAGERS_SESSION_SESSIONINFO_H
