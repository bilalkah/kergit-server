#include "app/commands/hub/CreateHubCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "domains/Hub.h"
#include "proto/command/hub.pb.h"
#include "proto/domain/channel.pb.h"
#include "proto/event/error.pb.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <optional>
#include <string>
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

    HubId hub_id;
    try {
        hub_id = ctx.hub_service.createHub(name, user_id);
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
        ctx.channel_service.createChannel(hub_id, "general", "text");
    } catch (...) {
    }

    ctx.subscription_manager.subscribeConnection(event->conn_id, Topic::HubTopic(hub_id));

    std::string avatar_seed;
    if (auto hub = ctx.hub_service.getHub(hub_id)) {
        avatar_seed = hub->avatar_seed;
    }

    std::optional<Channel> default_channel;
    const auto channels = ctx.channel_service.getHubChannels(hub_id);
    if (!channels.empty()) {
        default_channel = channels.front();
    }

    const auto self_role = ctx.hub_service.getMembershipRole(hub_id, user_id).value_or(Role::USER);
    std::string bytes =
        make_hub_create(hub_id, name, avatar_seed, user_id, self_role, true,
                                 default_channel);

    out.emplace_back(make_outgoing_message(net::outbound::Target::one(event->conn_id), std::move(bytes)));
    return out;
}

}  // namespace app
