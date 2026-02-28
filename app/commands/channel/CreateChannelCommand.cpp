#include "app/commands/channel/CreateChannelCommand.h"

#include "app/commands/utils.h"
#include "app/converters/ProtoConverters.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "domains/Channel.h"
#include "domains/Hub.h"
#include "proto/command/channel.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace app {

namespace {
std::string sanitize_name(std::string name) {
    auto trim = [](std::string& s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                                        [](unsigned char ch) { return !std::isspace(ch); }));
        s.erase(
            std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); })
                .base(),
            s.end());
    };
    trim(name);
    if (name.size() > 48) name.resize(48);
    return name;
}

std::string normalize_name(std::string name) {
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return name;
}
}  // namespace

std::vector<net::outbound::OutgoingMessage> CreateChannelCommand::execute(CommandContext& ctx,
                                                                          const queue::Event& evt) {
    const auto* event = std::get_if<queue::MessageEvent>(&evt);
    if (!event) {
        return {};
    }

    const auto& env = event->payload.env;
    if (env.type() != sercom::protocol::Envelope::CHANNEL_CREATE) {
        return {};
    }

    const auto& cmd = require_parsed<sercom::protocol::command::CreateChannel>(*event);

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!user_exp.has_value()) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
                                   "Authenticate first"));
    }
    const UserId user_id = user_exp.value();

    auto hub_id_opt = parse_wire_id<HubId>(cmd.hub_id());
    if (!hub_id_opt.has_value()) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_NOT_FOUND,
                                   "Hub not found"));
    }
    const HubId hub_id = hub_id_opt.value();

    std::string name = sanitize_name(cmd.name());
    if (name.empty()) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                                   "Channel name is required"));
    }

    if (!ctx.hub_service.isHubMember(hub_id, user_id)) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                   "Join the hub before creating channels"));
    }

    auto role = ctx.hub_service.getMembershipRole(hub_id, user_id);
    if (!role || (*role != Role::OWNER && *role != Role::ADMIN)) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                   "Only admins/owners can create channels"));
    }

    const auto existing = ctx.channel_service.getHubChannels(hub_id);
    const auto normalized = normalize_name(name);
    for (const auto& channel : existing) {
        if (normalize_name(channel.name) == normalized) {
            return single_outgoing(make_command_error(event->conn_id, env.type(),
                                       sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                                       "Channel name already exists"));
        }
    }

    const ChannelType channel_type = converters::from_proto_channel_type(cmd.type());
    const std::string type_str = channel_type == ChannelType::VOICE ? "voice" : "text";

    ChannelId created;
    try {
        created = ctx.channel_service.createChannel(hub_id, name, type_str);
    } catch (const std::exception& ex) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
                                   ex.what()));
    } catch (...) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
                                   "Unable to create channel"));
    }

    Channel created_channel{name, created, hub_id, channel_type};
    std::string bytes = ctx.hub_notifier.channelCreated(hub_id, created_channel);

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

    return single_outgoing(net::outbound::OutgoingMessage{
        .target = net::outbound::Target::many(std::move(conns)),
        .action =
            net::outbound::Action{std::in_place_type<net::outbound::SendPayload>,
                                  net::outbound::SendPayload{.payload = net::outbound::Payload{std::move(bytes), true}}}});
}

}  // namespace app
