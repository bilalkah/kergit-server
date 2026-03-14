#include "app/commands/activity/SelectActiveChannelCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "proto/command/activity.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"

#include <cassert>
#include <string_view>
#include <vector>

namespace app {

std::vector<net::outbound::OutgoingMessage> SelectActiveChannelCommand::execute(
    CommandContext& ctx, const queue::Event& evt) {
    std::vector<net::outbound::OutgoingMessage> out;

    assert(std::holds_alternative<queue::MessageEvent>(evt));
    const auto* event = &std::get<queue::MessageEvent>(evt);
    const auto& env = event->payload.env;
    assert(env.type() == sercom::protocol::Envelope::ACTIVE_CHANNEL);

    const auto& cmd = require_parsed<sercom::protocol::command::SelectActiveChannel>(*event);

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!user_exp.has_value()) {
        out.emplace_back(make_drop_connection(
            event->conn_id, sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
            "Unauthorized: No session associated with this connection"));
        return out;
    }

    const ChannelId channel_id{cmd.channel_id()};
    const HubId hub_id{cmd.hub_id()};

    auto channel_opt = ctx.channel_service.getChannel(channel_id);
    if (!channel_opt.has_value()) {
        out.emplace_back(make_command_error(event->conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_NOT_FOUND,
                                            "Channel not found"));
        return out;
    }

    if (channel_opt->hub_id != hub_id) {
        out.emplace_back(make_command_error(event->conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_NOT_FOUND,
                                            "Channel not found"));
        return out;
    }

    const UserId user_id = user_exp.value();
    if (!ctx.hub_service.isHubMember(hub_id, user_id)) {
        out.emplace_back(make_command_error(event->conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                            "Join the hub before selecting a channel"));
        return out;
    }

    ctx.subscription_manager.unsubscribeConnection(event->conn_id,
                                                   Topic::ChannelTopic(hub_id, channel_id));

    ctx.subscription_manager.subscribeConnection(event->conn_id,
                                                 Topic::ChannelTopic(hub_id, channel_id));
    ctx.session_manager.joinTextChannel(user_id, hub_id, channel_id);

    return out;
}

}  // namespace app
