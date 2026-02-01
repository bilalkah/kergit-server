#include "app/commands/activity/SelectActiveChannelCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "proto/command/activity.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"

#include <string_view>
#include <vector>

namespace app {

std::vector<net::outbound::OutgoingMessage> SelectActiveChannelCommand::execute(
    CommandContext& ctx, const queue::Event& evt) {
    const auto* event = std::get_if<queue::MessageEvent>(&evt);
    if (!event) {
        return {};
    }

    const auto& env = event->payload.env;
    if (env.type() != sercom::protocol::Envelope::ACTIVE_CHANNEL) {
        return {make_drop_connection(event->conn_id,
                                     sercom::protocol::event::CommandErrorCode_INVALID_FORMAT,
                                     "Invalid ACTIVE_CHANNEL envelope type")};
    }

    const auto* cmd = get_parsed<sercom::protocol::command::SelectActiveChannel>(*event);
    if (!cmd) {
        return {make_drop_connection(event->conn_id,
                                     sercom::protocol::event::CommandErrorCode_INVALID_FORMAT,
                                     "Invalid ACTIVE_CHANNEL payload")};
    }

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!user_exp.has_value()) {
        return {make_drop_connection(event->conn_id,
                                     sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
                                     "Authenticate first")};
    }
    const UserId user_id = user_exp.value();

    auto hub_id_opt = ctx.ids.to_internal(PublicHubId{cmd->hub_id()});
    if (!hub_id_opt.has_value()) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_NOT_FOUND,
                                   "Hub not found"));
    }

    auto channel_id_opt = ctx.ids.to_internal(PublicChannelId{cmd->channel_id()});
    if (!channel_id_opt.has_value()) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_NOT_FOUND,
                                   "Channel not found"));
    }

    auto channel_opt = ctx.channel_service.getChannel(*channel_id_opt);
    if (!channel_opt.has_value() || channel_opt->hub_id != *hub_id_opt) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_NOT_FOUND,
                                   "Channel not found"));
    }

    if (!ctx.hub_service.isHubMember(*hub_id_opt, user_id)) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                   "Join the hub before selecting a channel"));
    }

    auto session = ctx.session_manager.getSession(user_id);
    if (session.has_value() && session->current_text_channel && session->current_hub) {
        ctx.subscription_manager.unsubscribe(
            user_id, Topic::ChannelTopic(session->current_hub.value(),
                                         session->current_text_channel.value()));
    }

    ctx.subscription_manager.subscribe(user_id, Topic::ChannelTopic(*hub_id_opt, *channel_id_opt));
    ctx.session_manager.joinTextChannel(user_id, *hub_id_opt, *channel_id_opt);

    return {};
}

}  // namespace app
