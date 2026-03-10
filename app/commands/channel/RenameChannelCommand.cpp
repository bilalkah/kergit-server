#include "app/commands/channel/RenameChannelCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "domains/Channel.h"
#include "domains/Hub.h"
#include "proto/command/channel.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <vector>

namespace app {

namespace {
std::string normalize_name(std::string name) {
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return name;
}
}  // namespace

std::string RenameChannelCommand::sanitize(std::string name) {
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

std::vector<net::outbound::OutgoingMessage> RenameChannelCommand::execute(CommandContext& ctx,
                                                                          const queue::Event& evt) {
    const auto* event = std::get_if<queue::MessageEvent>(&evt);
    if (!event) {
        return {};
    }

    const auto& env = event->payload.env;
    if (env.type() != sercom::protocol::Envelope::CHANNEL_RENAME) {
        return {};
    }

    const auto& cmd = require_parsed<sercom::protocol::command::UpdateChannel>(*event);

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!user_exp.has_value()) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
            "Authenticate first"));
    }
    const UserId user_id = user_exp.value();

    auto channel_id_opt = parse_wire_id<ChannelId>(cmd.channel_id());
    if (!channel_id_opt.has_value()) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_NOT_FOUND,
            "Channel not found"));
    }

    auto channel_opt = ctx.channel_service.getChannel(*channel_id_opt);
    if (!channel_opt) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_NOT_FOUND,
            "Channel not found"));
    }
    const Channel channel = channel_opt.value();
    const HubId hub_id = channel.hub_id;

    std::optional<std::string> requested_name;
    for (int i = 0; i < cmd.changes_size(); ++i) {
        const auto& change = cmd.changes(i);
        switch (change.change_case()) {
            case sercom::protocol::command::ChannelChange::kName: {
                auto name = sanitize(change.name());
                if (name.empty()) {
                    return single_outgoing(make_command_error(
                        event->conn_id, env.type(),
                        sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                        "Channel name is required"));
                }
                requested_name = std::move(name);
                break;
            }
            case sercom::protocol::command::ChannelChange::CHANGE_NOT_SET:
            default:
                return single_outgoing(
                    make_command_error(event->conn_id, env.type(),
                                       sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                                       "Invalid change type"));
        }
    }

    if (!requested_name) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "No changes requested"));
    }

    if (!ctx.hub_service.isHubMember(hub_id, user_id)) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_FORBIDDEN,
            "Join the hub before renaming channels"));
    }

    auto role = ctx.hub_service.getMembershipRole(hub_id, user_id);
    if (!role || (*role != Role::OWNER && *role != Role::ADMIN)) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_FORBIDDEN,
            "Only admins/owners can rename channels"));
    }

    const auto existing = ctx.channel_service.getHubChannels(hub_id);
    const auto normalized = normalize_name(*requested_name);
    for (const auto& ch : existing) {
        if (ch.id == channel.id) continue;
        if (normalize_name(ch.name) == normalized) {
            return single_outgoing(
                make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                                   "Channel name already exists"));
        }
    }

    if (!ctx.channel_service.renameChannel(channel.id, *requested_name)) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
            "Unable to rename channel at this time"));
    }

    Channel renamed_channel = channel;
    renamed_channel.name = *requested_name;
    std::string bytes = ctx.hub_notifier.channelRenamed(renamed_channel);

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
                                  net::outbound::SendPayload{
                                      .payload = net::outbound::Payload{std::move(bytes), true}}}});
}

}  // namespace app
