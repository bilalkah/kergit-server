#include "app/commands/channel/RenameChannelCommand.h"

#include "app/commands/utils.h"
#include "app/converters/ProtoConverters.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "domains/Channel.h"
#include "domains/Hub.h"
#include "proto/command/channel.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/channel.pb.h"
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

std::vector<net::outbound::OutgoingMessage> RenameChannelCommand::execute(
    CommandContext& ctx, const queue::Event& evt) {
    const auto* event = std::get_if<queue::MessageEvent>(&evt);
    if (!event) {
        return {};
    }

    const auto& env = event->payload.env;
    if (env.type() != sercom::protocol::Envelope::CHANNEL_RENAME) {
        return {};
    }

    sercom::protocol::command::UpdateChannel cmd;
    if (!cmd.ParseFromString(env.payload())) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INVALID_FORMAT,
                                   "Invalid CHANNEL_RENAME payload")};
    }

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!user_exp.has_value()) {
        return {make_command_error(event->conn_id, env.type(),
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
    const HubId hub_id = hub_id_opt.value();

    auto channel_id_opt = ctx.ids.to_internal(PublicChannelId{cmd.channel_id()});
    if (!channel_id_opt.has_value()) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_NOT_FOUND,
                                   "Channel not found")};
    }

    auto channel_opt = ctx.channel_service.getChannel(*channel_id_opt);
    if (!channel_opt.has_value() || channel_opt->hub_id != hub_id) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_NOT_FOUND,
                                   "Channel not found")};
    }
    const Channel channel = channel_opt.value();

    std::optional<std::string> requested_name;
    for (int i = 0; i < cmd.changes_size(); ++i) {
        const auto& change = cmd.changes(i);
        switch (change.change_case()) {
            case sercom::protocol::command::ChannelChange::kName: {
                auto name = sanitize(change.name());
                if (name.empty()) {
                    return {make_command_error(event->conn_id, env.type(),
                                               sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                                               "Channel name is required")};
                }
                requested_name = std::move(name);
                break;
            }
            case sercom::protocol::command::ChannelChange::CHANGE_NOT_SET:
            default:
                return {make_command_error(event->conn_id, env.type(),
                                           sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                                           "Invalid change type")};
        }
    }

    if (!requested_name.has_value()) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                                   "No changes requested")};
    }

    if (!ctx.hub_service.isHubMember(hub_id, user_id)) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                   "Join the hub before renaming channels")};
    }

    auto role = ctx.hub_service.getMembershipRole(hub_id, user_id);
    if (!role.has_value() || (*role != Role::OWNER && *role != Role::ADMIN)) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                   "Only admins/owners can rename channels")};
    }

    const auto existing = ctx.channel_service.getHubChannels(hub_id);
    const auto normalized = normalize_name(*requested_name);
    for (const auto& ch : existing) {
        if (ch.id == channel.id) continue;
        if (normalize_name(ch.name) == normalized) {
            return {make_command_error(event->conn_id, env.type(),
                                       sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                                       "Channel name already exists")};
        }
    }

    if (!ctx.channel_service.renameChannel(channel.id, *requested_name)) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
                                   "Unable to rename channel at this time")};
    }

    sercom::protocol::event::ChannelRenamed renamed_evt;
    renamed_evt.set_hub_id(ctx.ids.to_public(hub_id).value);
    auto* out_channel = renamed_evt.mutable_channel();
    out_channel->set_id(ctx.ids.to_public(channel.id).value);
    out_channel->set_name(*requested_name);
    out_channel->set_type(converters::to_proto_channel_type(channel.type));

    sercom::protocol::Envelope out_env;
    out_env.set_version(1);
    out_env.set_type(sercom::protocol::Envelope::CHANNEL_RENAMED);
    renamed_evt.SerializeToString(out_env.mutable_payload());

    std::string bytes;
    out_env.SerializeToString(&bytes);

    auto subs = ctx.subscription_manager.getSubscribers(Topic::HubTopic(hub_id));
    if (!subs.has_value() || subs->empty()) {
        return {};
    }

    std::vector<GlobalConnId> conns;
    conns.reserve(subs->size());
    for (const auto& uid : subs.value()) {
        auto conn = ctx.session_manager.getMainConnection(uid);
        if (conn.has_value()) conns.push_back(conn.value());
    }
    if (conns.empty()) {
        return {};
    }

    return {net::outbound::OutgoingMessage{
        .target = net::outbound::Target::many(std::move(conns)),
        .action = net::outbound::SendPayload{
            .payload = net::outbound::Payload{.data = std::move(bytes), .is_binary = true}}}};
}

}  // namespace app
