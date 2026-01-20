#include "app/commands/channel/CreateChannelCommand.h"

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

    sercom::protocol::command::CreateChannel cmd;
    if (!cmd.ParseFromString(env.payload())) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INVALID_FORMAT,
                                   "Invalid CHANNEL_CREATE payload")};
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

    std::string name = sanitize_name(cmd.name());
    if (name.empty()) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                                   "Channel name is required")};
    }

    if (!ctx.hub_service.isHubMember(hub_id, user_id)) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                   "Join the hub before creating channels")};
    }

    auto role = ctx.hub_service.getMembershipRole(hub_id, user_id);
    if (!role.has_value() || (*role != Role::OWNER && *role != Role::ADMIN)) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                   "Only admins/owners can create channels")};
    }

    const auto existing = ctx.channel_service.getHubChannels(hub_id);
    const auto normalized = normalize_name(name);
    for (const auto& channel : existing) {
        if (normalize_name(channel.name) == normalized) {
            return {make_command_error(event->conn_id, env.type(),
                                       sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                                       "Channel name already exists")};
        }
    }

    const ChannelType channel_type = converters::from_proto_channel_type(cmd.type());
    const std::string type_str = channel_type == ChannelType::VOICE ? "voice" : "text";

    ChannelId created;
    try {
        created = ctx.channel_service.createChannel(hub_id, name, type_str);
    } catch (const std::exception& ex) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
                                   ex.what())};
    } catch (...) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
                                   "Unable to create channel")};
    }

    sercom::protocol::event::ChannelCreated created_evt;
    created_evt.set_hub_id(ctx.ids.to_public(hub_id).value);
    auto* out_channel = created_evt.mutable_channel();
    out_channel->set_id(ctx.ids.to_public(created).value);
    out_channel->set_name(name);
    out_channel->set_type(converters::to_proto_channel_type(channel_type));

    sercom::protocol::Envelope out_env;
    out_env.set_version(1);
    out_env.set_type(sercom::protocol::Envelope::CHANNEL_CREATED);
    created_evt.SerializeToString(out_env.mutable_payload());

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
