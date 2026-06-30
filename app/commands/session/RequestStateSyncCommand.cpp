#include "app/commands/session/RequestStateSyncCommand.h"

#include "app/commands/session/StateSyncBuilder.h"
#include "app/commands/utils.h"
#include "proto/command/session.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"
#include "utils/EventLogger.h"

#include <unordered_set>
#include <variant>

namespace app {

std::vector<net::outbound::OutgoingMessage> RequestStateSyncCommand::execute(
    CommandContext& ctx, const queue::Event& evt) {
    std::vector<net::outbound::OutgoingMessage> out;
    const auto* event = std::get_if<queue::MessageEvent>(&evt);
    if (!event) {
        utils::EventLogger::instance().log(utils::EventCategory::DEBUG, "",
                                           "REQUEST_STATE_SYNC_INVALID", 0,
                                           "RequestStateSyncCommand: invalid event type");
        return out;
    }

    const auto* cmd =
        std::get_if<sercom::protocol::command::RequestStateSync>(&event->payload.parsed);
    if (!cmd) {
        utils::EventLogger::instance().log(utils::EventCategory::DEBUG, "",
                                           "REQUEST_STATE_SYNC_INVALID", 0,
                                           "RequestStateSyncCommand: invalid payload type");
        return out;
    }

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!user_exp.has_value()) {
        out.emplace_back(make_drop_connection(
            event->conn_id,
            sercom::protocol::event::CommandErrorCode::CommandErrorCode_UNAUTHORIZED,
            "Authenticate first"));
        return out;
    }
    const UserId user_id = user_exp.value();

    sercom::protocol::event::StateSync sync;
    if (cmd->hub_ids_size() == 0) {
        sync = build_state_sync_for_user(ctx, user_id);
    } else {
        std::unordered_set<HubId> requested_hub_ids;
        requested_hub_ids.reserve(static_cast<size_t>(cmd->hub_ids_size()));
        for (const auto& hub_id_raw : cmd->hub_ids()) {
            if (hub_id_raw.empty()) {
                continue;
            }
            requested_hub_ids.insert(HubId{hub_id_raw});
        }
        sync = build_state_sync_for_requested_hubs(ctx, user_id, requested_hub_ids);
    }

    out.emplace_back(
        make_outgoing_message(net::outbound::Target::one(event->conn_id), make_state_sync(sync)));
    return out;
}

}  // namespace app
