#include "app/commands/channel/RenameChannelCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "domains/Channel.h"
#include "domains/Hub.h"
#include "proto/command/channel.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"
#include "proto/event/state.pb.h"

#include <cassert>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace app {

namespace {
constexpr std::size_t kMaxChannelNameLength = 80;  // matches channels_name_length SQL constraint
}  // namespace

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
        out.emplace_back(make_command_error(event->conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
                                            "Authenticate first"));
        return out;
    }

    const UserId user_id = user_exp.value();
    const auto scope_opt = to_channel_scope(cmd.channel());
    if (!scope_opt.has_value()) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "channel.hub_id and channel.channel_id are required"));
        return out;
    }
    const ChannelId channel_id = scope_opt->channel_id;
    const HubId requested_hub_id = scope_opt->hub_id;
    auto channel_opt = ctx.hub_service.getChannel(channel_id);
    if (!channel_opt) {
        out.emplace_back(make_command_error(event->conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_NOT_FOUND,
                                            "Channel not found"));
        return out;
    }
    const Channel channel = channel_opt.value();
    const HubId hub_id = channel.hub_id;
    if (hub_id != requested_hub_id) {
        out.emplace_back(make_command_error(event->conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_NOT_FOUND,
                                            "Channel not found"));
        return out;
    }

    if (!cmd.has_name()) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "No changes requested"));
        return out;
    }

    std::string requested_name = sanitize(cmd.name());
    if (requested_name.empty()) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "Channel name is required"));
        return out;
    }
    // Reject (do not truncate) names longer than the SQL baseline.
    if (utf8_length(requested_name) > kMaxChannelNameLength) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "Channel name must be at most 80 characters"));
        return out;
    }

    auto role = ctx.hub_service.getMembershipRole(hub_id, user_id);
    if (!role) {
        out.emplace_back(make_command_error(event->conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                            "Join the hub before renaming channels"));
        return out;
    }

    if (*role != Role::OWNER && *role != Role::ADMIN) {
        out.emplace_back(make_command_error(event->conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                            "Only admins/owners can rename channels"));
        return out;
    }

    const auto channel_ids = ctx.hub_service.getHubChannelIds(hub_id);
    const auto existing = ctx.hub_service.getChannelsByIds(channel_ids);
    const auto normalized = normalize_name_lowercase(requested_name);
    for (const auto& [_, ch] : existing) {
        if (ch.id == channel.id) continue;
        if (normalize_name_lowercase(ch.name) == normalized) {
            out.emplace_back(
                make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                                   "Channel name already exists"));
            return out;
        }
    }

    try {
        if (!ctx.hub_service.renameChannel(channel.id, requested_name)) {
            out.emplace_back(
                make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
                                   "Unable to rename channel at this time"));
            return out;
        }

        ctx.audit_service.log(AuditRepository::Event{
            .category = "channel",
            .event_type = "channel.updated",
            .severity = "info",
            .actor_type = "user",
            .actor_user_id = user_id,
            .hub_id = hub_id,
            .channel_id = channel.id,
            .session_id = std::to_string(
                ctx.session_manager.sessionIdOfConnection(event->conn_id).value_or(0)),
            .connection_id = to_string(event->conn_id),
        });
    } catch (const std::exception& ex) {
        // Map known DB constraint errors (e.g. duplicate-name race) to a clean
        // message; never leak the raw DB error to the client.
        const auto mapped = map_channel_write_error(ex.what());
        out.emplace_back(make_command_error(
            event->conn_id, env.type(),
            mapped ? sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT
                   : sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
            mapped ? *mapped : std::string{"Unable to rename channel at this time"}));
        return out;
    }

    Channel renamed_channel = channel;
    renamed_channel.name = requested_name;
    sercom::protocol::event::StateDelta delta;
    auto* hub_delta = delta.add_hubs();
    hub_delta->set_hub_id(hub_id.value);
    auto* channel_delta = hub_delta->add_channels();
    channel_delta->set_channel_id(channel.id.value);
    auto* upsert = channel_delta->add_channel_ops()->mutable_upsert();
    *upsert->mutable_channel() = to_proto_channel(renamed_channel);

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

    out.emplace_back(make_outgoing_message(net::outbound::Target::many(std::move(conns)),
                                           make_state_delta(delta)));
    return out;
}

}  // namespace app
