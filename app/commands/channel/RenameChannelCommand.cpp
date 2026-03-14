#include "app/commands/channel/RenameChannelCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "domains/Channel.h"
#include "domains/Hub.h"
#include "proto/command/channel.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"

#include <cassert>
#include <optional>
#include <string>
#include <vector>

namespace app {

std::vector<net::outbound::OutgoingMessage> RenameChannelCommand::execute(CommandContext& ctx,
                                                                          const queue::Event& evt) {
    std::vector<net::outbound::OutgoingMessage> out;
    assert(std::holds_alternative<queue::MessageEvent>(evt));
    const auto* event = &std::get<queue::MessageEvent>(evt);
    const auto& env = event->payload.env;
    assert(env.type() == sercom::protocol::Envelope::CHANNEL_UPDATE);

    const auto& cmd = require_parsed<sercom::protocol::command::UpdateChannel>(*event);

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!user_exp.has_value()) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
            "Authenticate first"));
        return out;
    }
    
    const UserId user_id = user_exp.value();
    const ChannelId channel_id{cmd.channel_id()};
    auto channel_opt = ctx.channel_service.getChannel(channel_id);
    if (!channel_opt) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_NOT_FOUND,
            "Channel not found"));
        return out;
    }
    const Channel channel = channel_opt.value();
    const HubId hub_id = channel.hub_id;

    if (!cmd.has_name()) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "No changes requested"));
        return out;
    }

    std::string requested_name = sanitize(cmd.name());
    if (requested_name.size() > 48) requested_name.resize(48);
    if (requested_name.empty()) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "Channel name is required"));
        return out;
    }

    if (!ctx.hub_service.isHubMember(hub_id, user_id)) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_FORBIDDEN,
            "Join the hub before renaming channels"));
        return out;
    }

    auto role = ctx.hub_service.getMembershipRole(hub_id, user_id);
    if (!role || (*role != Role::OWNER && *role != Role::ADMIN)) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_FORBIDDEN,
            "Only admins/owners can rename channels"));
        return out;
    }

    const auto existing = ctx.channel_service.getHubChannels(hub_id);
    const auto normalized = normalize_name_lowercase(requested_name);
    for (const auto& ch : existing) {
        if (ch.id == channel.id) continue;
        if (normalize_name_lowercase(ch.name) == normalized) {
            out.emplace_back(
                make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                                   "Channel name already exists"));
            return out;
        }
    }

    if (!ctx.channel_service.renameChannel(channel.id, requested_name)) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
            "Unable to rename channel at this time"));
        return out;
    }

    Channel renamed_channel = channel;
    renamed_channel.name = requested_name;
    std::string bytes = make_channel_update(hub_id, renamed_channel);

    utils::metrics::counters().fanout_subscriber_snapshot_total.fetch_add(
        1, std::memory_order_relaxed);
    auto subs = ctx.subscription_manager.getSubscribers(Topic::HubTopic(hub_id));
    if (!subs || subs->empty()) {
        return {};
    }

    std::vector<GlobalConnId> conns;
    conns.reserve(subs->size());
    for (const auto& conn : *subs) {
        conns.push_back(conn);
    }
    if (conns.empty()) {
        return {};
    }

    out.emplace_back(make_outgoing_message(net::outbound::Target::many(std::move(conns)), std::move(bytes)));
    return out;
}

}  // namespace app
