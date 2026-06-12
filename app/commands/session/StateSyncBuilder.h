#ifndef APP_COMMANDS_SESSION_STATESYNCBUILDER_H
#define APP_COMMANDS_SESSION_STATESYNCBUILDER_H

#include "app/dispatcher/CommandContext.h"
#include "proto/event/state.pb.h"

#include <unordered_set>

namespace app {

sercom::protocol::event::StateSync build_state_sync_for_user(CommandContext& ctx,
                                                             const UserId& user_id);

sercom::protocol::event::StateSync build_state_sync_for_requested_hubs(
    CommandContext& ctx, const UserId& user_id, const std::unordered_set<HubId>& requested_hub_ids);

}  // namespace app

#endif  // APP_COMMANDS_SESSION_STATESYNCBUILDER_H
