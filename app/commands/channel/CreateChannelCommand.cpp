#include "app/commands/channel/CreateChannelCommand.h"

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
#include <string>
#include <vector>

namespace app {

std::vector<net::outbound::OutgoingMessage> CreateChannelCommand::execute(CommandContext& ctx,
                                                                          const queue::Event& evt) {
    std::vector<net::outbound::OutgoingMessage> out;
    assert(std::holds_alternative<queue::MessageEvent>(evt));
    const auto* event = &std::get<queue::MessageEvent>(evt);
    const auto& env = event->payload.env;
    assert(env.type() == sercom::protocol::Envelope::CHANNEL_CREATE);

    const auto& cmd = require_parsed<sercom::protocol::command::CreateChannel>(*event);

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!user_exp.has_value()) {
        out.emplace_back(make_command_error(event->conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
                                            "Authenticate first"));
        return out;
    }
    const UserId user_id = user_exp.value();

    const HubId hub_id{cmd.hub_id()};

    std::string name = sanitize(cmd.name());
    if (name.size() > 48) name.resize(48);
    if (name.empty()) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "Channel name is required"));
        return out;
    }

    auto role = ctx.hub_service.getMembershipRole(hub_id, user_id);
    if (!role) {
        out.emplace_back(make_command_error(event->conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                            "Join the hub before creating channels"));
        return out;
    }

    if (*role != Role::OWNER && *role != Role::ADMIN) {
        out.emplace_back(make_command_error(event->conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                            "Only admins/owners can create channels"));
        return out;
    }

    const auto channel_ids = ctx.hub_service.getHubChannelIds(hub_id);
    const auto existing = ctx.hub_service.getChannelsByIds(channel_ids);
    const auto normalized = normalize_name_lowercase(name);
    for (const auto& [_, channel] : existing) {
        if (normalize_name_lowercase(channel.name) == normalized) {
            out.emplace_back(
                make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                                   "Channel name already exists"));
            return out;
        }
    }

    const ChannelType channel_type = from_proto_channel_type(cmd.type());
    const std::string type_str = channel_type == ChannelType::VOICE ? "voice" : "text";

    ChannelId created;
    try {
        created = ctx.hub_service.createChannel(hub_id, name, type_str);
    } catch (const std::exception& ex) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
            ex.what()));
        return out;
    } catch (...) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
            "Unable to create channel"));
        return out;
    }

    Channel created_channel{name, created, hub_id, channel_type};

    sercom::protocol::event::StateDelta delta;
    auto* hub_delta = delta.add_hubs();
    hub_delta->set_hub_id(hub_id.value);
    auto* channel_delta = hub_delta->add_channels();
    channel_delta->set_channel_id(created.value);
    auto* upsert = channel_delta->add_channel_ops()->mutable_upsert();
    *upsert->mutable_channel() = to_proto_channel(created_channel);

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

    out.emplace_back(
        make_outgoing_message(net::outbound::Target::many(std::move(conns)), make_state_delta(delta)));
    return out;
}

}  // namespace app
