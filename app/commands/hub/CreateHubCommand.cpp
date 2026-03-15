#include "app/commands/hub/CreateHubCommand.h"

#include "app/commands/session/StateSyncBuilder.h"
#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "proto/command/hub.pb.h"
#include "proto/event/error.pb.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <string>
#include <unordered_set>
#include <vector>

namespace app {

std::vector<net::outbound::OutgoingMessage> CreateHubCommand::execute(CommandContext& ctx,
                                                                      const queue::Event& evt) {
    std::vector<net::outbound::OutgoingMessage> out;
    assert(std::holds_alternative<queue::MessageEvent>(evt));
    const auto* event = &std::get<queue::MessageEvent>(evt);
    const auto& env = event->payload.env;
    assert(env.type() == sercom::protocol::Envelope::HUB_CREATE);

    const auto& cmd = require_parsed<sercom::protocol::command::CreateHub>(*event);

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!user_exp.has_value()) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
            "Authenticate first"));
        return out;
    }
    const UserId user_id = user_exp.value();

    std::string name = sanitize(cmd.name());
    if (name.size() > 64) name.resize(64);
    if (name.empty()) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "Hub name is required"));
        return out;
    }

    Hub created_hub;
    try {
        created_hub = ctx.hub_service.createHub(name, user_id);
    } catch (const std::exception& ex) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
            ex.what()));
        return out;
    } catch (...) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
            "Failed to create hub"));
        return out;
    }

    try {
        ctx.hub_service.createChannel(created_hub.id, "general", "text");
    } catch (const std::exception& ex) {
        utils::log_line(
            utils::LogLevel::WARN,
            std::string("CreateHubCommand: default channel creation failed for hub_id=") +
                created_hub.id.value + " error=" + ex.what());
    } catch (...) {
        utils::log_line(
            utils::LogLevel::WARN,
            std::string("CreateHubCommand: default channel creation failed for hub_id=") +
                created_hub.id.value + " error=unknown");
    }

    ctx.subscription_manager.subscribeConnection(event->conn_id, Topic::HubTopic(created_hub.id));

    std::unordered_set<HubId> requested_hub_ids{created_hub.id};
    const auto sync = build_state_sync_for_requested_hubs(ctx, user_id, requested_hub_ids);
    out.emplace_back(
        make_outgoing_message(net::outbound::Target::one(event->conn_id), make_state_sync(sync)));
    return out;
}

}  // namespace app
