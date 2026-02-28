#ifndef APP_MANAGERS_SESSION_SESSIONINFO_H
#define APP_MANAGERS_SESSION_SESSIONINFO_H

#include "app/managers/session/SessionId.h"
#include "domains/ids/Ids.h"

#include <optional>
#include <unordered_set>
namespace app {

struct SessionInfo {
    // application-level identifiers
    std::unordered_set<HubId> snapshotted_hubs;

    // current context
    std::optional<HubId> current_hub;
    std::optional<ChannelId> current_voice_channel;
    std::optional<SessionId> voice_owner_session;
    std::optional<ChannelId> pending_voice_channel;
    std::optional<SessionId> pending_voice_owner_session;
    bool voice_muted = false;
    bool voice_deafened = false;
};

}  // namespace app

#endif  // APP_MANAGERS_SESSION_SESSIONINFO_H
