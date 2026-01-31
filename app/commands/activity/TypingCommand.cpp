#include "app/commands/activity/TypingCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "proto/command/activity.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"
#include "proto/event/presence.pb.h"

#include <string_view>
#include <vector>

namespace app {

std::vector<net::outbound::OutgoingMessage> TypingCommand::execute(CommandContext& ctx,
                                                                   const queue::Event& evt) {
    const auto* event = std::get_if<queue::MessageEvent>(&evt);
    if (!event) {
        return {};
    }

    const auto& env = event->payload.env;
    if (env.type() != sercom::protocol::Envelope::TYPING) {
        return {make_drop_connection(event->conn_id,
                                     sercom::protocol::event::CommandErrorCode_INVALID_FORMAT,
                                     "Invalid TYPING envelope type")};
    }

    sercom::protocol::command::Typing cmd;
    if (!cmd.ParseFromString(env.payload())) {
        return {make_drop_connection(event->conn_id,
                                     sercom::protocol::event::CommandErrorCode_INVALID_FORMAT,
                                     "Invalid TYPING payload")};
    }

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!user_exp.has_value()) {
        return {make_drop_connection(event->conn_id,
                                     sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
                                     "Authenticate first")};
    }
    const UserId user_id = user_exp.value();

    auto hub_id_opt = ctx.ids.to_internal(PublicHubId{cmd.hub_id()});
    if (!hub_id_opt.has_value()) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_NOT_FOUND,
                                   "Hub not found")};
    }

    auto channel_id_opt = ctx.ids.to_internal(PublicChannelId{cmd.channel_id()});
    if (!channel_id_opt.has_value()) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_NOT_FOUND,
                                   "Channel not found")};
    }

    if (!ctx.hub_service.isHubMember(*hub_id_opt, user_id)) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                   "Join the hub before typing")};
    }

    auto session = ctx.session_manager.getSession(user_id);
    if (!session.has_value() || !session->current_text_channel || !session->current_hub ||
        session->current_text_channel.value() != *channel_id_opt ||
        session->current_hub.value() != *hub_id_opt) {
        return {};
    }

    sercom::protocol::event::PresenceEvent presence;
    if (cmd.state() == sercom::protocol::command::Typing::STATE_STARTED) {
        auto* payload = presence.mutable_typing_started();
        payload->set_hub_id(ctx.ids.to_public(*hub_id_opt).value);
        payload->set_user_id(ctx.ids.to_public(user_id).value);
        payload->set_channel_id(ctx.ids.to_public(*channel_id_opt).value);
    } else if (cmd.state() == sercom::protocol::command::Typing::STATE_STOPPED) {
        auto* payload = presence.mutable_typing_stopped();
        payload->set_hub_id(ctx.ids.to_public(*hub_id_opt).value);
        payload->set_user_id(ctx.ids.to_public(user_id).value);
        payload->set_channel_id(ctx.ids.to_public(*channel_id_opt).value);
    } else {
        return {};
    }

    std::string presence_payload;
    presence.SerializeToString(&presence_payload);

    sercom::protocol::Envelope out_env;
    out_env.set_version(1);
    out_env.set_type(sercom::protocol::Envelope::PRESENCE);
    out_env.set_payload(std::move(presence_payload));

    std::string bytes;
    out_env.SerializeToString(&bytes);

    std::vector<GlobalConnId> conns;
    auto subs =
        ctx.subscription_manager.getSubscribers(Topic::ChannelTopic(*hub_id_opt, *channel_id_opt));
    if (subs.has_value()) {
        for (const auto& uid : subs.value()) {
            if (uid == user_id) continue;
            auto conn = ctx.session_manager.getMainConnection(uid);
            if (conn.has_value()) {
                conns.push_back(conn.value());
            }
        }
    }

    if (conns.empty()) {
        return {};
    }

    return {net::outbound::OutgoingMessage{
        .priority = net::outbound::OutboundPriority::Low,
        .target = net::outbound::Target::many(std::move(conns)),
        .action = net::outbound::SendPayload{
            .payload = net::outbound::Payload{.data = std::move(bytes), .is_binary = true}}}};
}

}  // namespace app
