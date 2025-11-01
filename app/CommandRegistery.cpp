#include "app/CommandRegistry.h"
#include "app/commands/PingCommand.h"

namespace app {
void register_all(Dispatcher& d) {
    d.register_cmd("ping", std::make_unique<PingCommand>());
    // later: list_hubs, channels_for_hub, join_channel, send_message, ...
}
}  // namespace app
