#ifndef APP_MANAGERS_SESSION_SESSIONINFO_H
#define APP_MANAGERS_SESSION_SESSIONINFO_H

#include "domains/ids/Ids.h"

#include <optional>

namespace app {

struct SessionInfo {
    // current context
    std::optional<HubId> current_hub;
    std::optional<ChannelId> current_text_channel;
    std::optional<ChannelId> current_voice_channel;

    // transport-level connections
    std::optional<GlobalConnId> text_conn;
    std::optional<GlobalConnId> voice_conn;
};

}  // namespace app

#endif  // APP_MANAGERS_SESSION_SESSIONINFO_H
