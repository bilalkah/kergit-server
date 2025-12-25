#include "app/commands/system/DisconnectCommand.h"

#include "app/services/HubPublisher.h"
#include "net/ClientGateway.h"

#include <nlohmann/json.hpp>

namespace app {

DisconnectCommand::DisconnectCommand(ServiceObjects& svc_objs)
    : services_(svc_objs) {}

void DisconnectCommand::execute(CommandContext& ctx) {
    services_.gateway_.unsubscribe_all(ctx.conn_id);
    services_.hub_publisher_.publish_hubs(ctx.snapshot.hubs);
    services_.cache_.remove(ctx.snapshot.user_id);
}

}  // namespace app
