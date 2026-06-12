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
#include <cstddef>
#include <string>
#include <unordered_set>
#include <vector>

namespace app {

namespace {
constexpr std::size_t kMaxHubNameLength = 80;  // matches hubs_name_length SQL constraint
}  // namespace

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
        out.emplace_back(make_command_error(event->conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
                                            "Authenticate first"));
        return out;
    }
    const UserId user_id = user_exp.value();

    std::string name = sanitize(cmd.name());
    if (name.empty()) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "Hub name is required"));
        return out;
    }
    // Reject (do not truncate) names longer than the SQL baseline.
    if (utf8_length(name) > kMaxHubNameLength) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "Hub name must be at most 80 characters"));
        return out;
    }

    Hub created_hub;
    try {
        created_hub = ctx.hub_service.createHub(name, user_id);
    } catch (const std::exception& ex) {
        const auto mapped = map_hub_write_error(ex.what());
        out.emplace_back(
            make_command_error(event->conn_id, env.type(),
                               mapped ? sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT
                                      : sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
                               mapped ? *mapped : std::string{"Failed to create hub"}));
        return out;
    } catch (...) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
            "Failed to create hub"));
        return out;
    }

    try {
        ctx.hub_service.createChannel(created_hub.id, "general", "text", user_id);
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

    auto self_conns = ctx.session_manager.getSessionConnections(user_id);
    if (self_conns.empty()) {
        self_conns.push_back(event->conn_id);
    }
    for (const auto& conn : self_conns) {
        ctx.subscription_manager.subscribeConnection(conn, Topic::HubTopic(created_hub.id));
    }

    std::unordered_set<HubId> requested_hub_ids{created_hub.id};
    const auto sync = build_state_sync_for_requested_hubs(ctx, user_id, requested_hub_ids);
    out.emplace_back(make_outgoing_message(net::outbound::Target::many(std::move(self_conns)),
                                           make_state_sync(sync)));
    return out;
}

}  // namespace app
