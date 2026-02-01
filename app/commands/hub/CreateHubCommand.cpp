#include "app/commands/hub/CreateHubCommand.h"

#include "app/commands/utils.h"
#include "app/converters/ProtoConverters.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "domains/Hub.h"
#include "proto/command/hub.pb.h"
#include "proto/domain/channel.pb.h"
#include "proto/domain/hub.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"
#include "proto/event/hub.pb.h"

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
    if (name.size() > 64) name.resize(64);
    return name;
}

std::string resolve_display_name(CommandContext& ctx, const UserId& user_id,
                                 const std::string& stored_display) {
    if (!stored_display.empty()) return stored_display;
    if (auto user = ctx.user_service.getUser(user_id)) {
        if (!user->username.empty()) return user->username;
        if (!user->full_name.empty()) return user->full_name;
    }
    return "Member";
}

sercom::protocol::domain::HubRole role_to_proto(const std::optional<Role>& role) {
    if (!role.has_value()) return sercom::protocol::domain::HubRole_MEMBER;
    return converters::to_proto_hub_role(*role);
}

}  // namespace

std::vector<net::outbound::OutgoingMessage> CreateHubCommand::execute(CommandContext& ctx,
                                                                      const queue::Event& evt) {
    const auto* event = std::get_if<queue::MessageEvent>(&evt);
    if (!event) {
        return {};
    }

    const auto& env = event->payload.env;
    if (env.type() != sercom::protocol::Envelope::HUB_CREATE) {
        return {};
    }

    const auto* cmd = get_parsed<sercom::protocol::command::CreateHub>(*event);
    if (!cmd) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INVALID_FORMAT,
                                   "Invalid HUB_CREATE payload"));
    }

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!user_exp.has_value()) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
                                   "Authenticate first"));
    }
    const UserId user_id = user_exp.value();

    std::string name = sanitize_name(cmd->name());
    if (name.empty()) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                                   "Hub name is required"));
    }

    HubId hub_id;
    try {
        hub_id = ctx.hub_service.createHub(name, user_id);
    } catch (const std::exception& ex) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
                                   ex.what()));
    } catch (...) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
                                   "Failed to create hub"));
    }

    try {
        ctx.channel_service.createChannel(hub_id, "general", "text");
    } catch (...) {
    }

    ctx.subscription_manager.subscribeConnection(event->conn_id, Topic::HubTopic(hub_id));

    const auto public_hub_id = ctx.ids.to_public(hub_id).value;
    const auto public_user_id = ctx.ids.to_public(user_id).value;

    sercom::protocol::event::HubCreated created;
    created.mutable_hub()->set_id(public_hub_id);
    created.mutable_hub()->set_name(name);
    if (auto hub = ctx.hub_service.getHub(hub_id)) {
        created.mutable_hub()->set_avatar_seed(hub->avatar_seed);
    }

    auto* self = created.mutable_self_member();
    self->set_hub_id(public_hub_id);
    self->set_user_id(public_user_id);
    self->set_is_online(true);
    self->set_role(role_to_proto(ctx.hub_service.getMembershipRole(hub_id, user_id)));
    self->set_display_name(resolve_display_name(ctx, user_id, ""));
    if (auto user = ctx.user_service.getUser(user_id)) {
        self->set_avatar_seed(user->avatar_seed);
    }

    const auto channels = ctx.channel_service.getHubChannels(hub_id);
    if (!channels.empty()) {
        const auto& channel = channels.front();
        auto* ch = created.mutable_channel();
        ch->set_id(ctx.ids.to_public(channel.id).value);
        ch->set_name(channel.name);
        ch->set_type(converters::to_proto_channel_type(channel.type));
    }

    sercom::protocol::Envelope out_env;
    out_env.set_version(1);
    out_env.set_type(sercom::protocol::Envelope::HUB_CREATED);
    created.SerializeToString(out_env.mutable_payload());

    std::string bytes;
    out_env.SerializeToString(&bytes);

    return single_outgoing(net::outbound::OutgoingMessage{
        .target = net::outbound::Target::one(event->conn_id),
        .action =
            net::outbound::Action{std::in_place_type<net::outbound::SendPayload>,
                                  net::outbound::SendPayload{.payload = net::outbound::Payload{std::move(bytes), true}}}});
}

}  // namespace app
