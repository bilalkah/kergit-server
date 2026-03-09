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
};

}  // namespace app

#endif  // APP_MANAGERS_SESSION_SESSIONINFO_H
