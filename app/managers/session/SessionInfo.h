#ifndef APP_MANAGERS_SESSION_SESSIONINFO_H
#define APP_MANAGERS_SESSION_SESSIONINFO_H

#include "domains/ids/Ids.h"

#include <optional>
#include <unordered_set>
namespace app {

struct SessionInfo {
    // current context
    std::optional<HubId> current_hub;
    std::optional<ChannelId> current_channel;
};

}  // namespace app

#endif  // APP_MANAGERS_SESSION_SESSIONINFO_H
