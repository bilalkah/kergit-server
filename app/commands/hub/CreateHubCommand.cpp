#include "app/commands/hub/CreateHubCommand.h"

#include "app/commands/utils.h"
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
#include <cassert>
#include <cctype>
#include <string>
#include <vector>

namespace app {

std::vector<net::outbound::OutgoingMessage> CreateHubCommand::execute(CommandContext& ctx,
                                                                      const queue::Event& evt) {
    std::vector<net::outbound::OutgoingMessage> out;
    assert(std::holds_alternative<queue::MessageEvent>(evt));
    const auto* event = &std::get<queue::MessageEvent>(evt);
    const auto& env = event->payload.env;
    assert(env.type() == sercom::protocol::Envelope::HUB_CREATE);

    const auto& cmd = require_parsed<sercom::protocol::command::CreateHub>(*event);

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!user_exp.has_value()) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
            "Authenticate first"));
        return out;
    }
    const UserId user_id = user_exp.value();

    std::string name = sanitize(cmd.name());
    if (name.size() > 64) name.resize(64);
    if (name.empty()) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "Hub name is required"));
        return out;
    }

    HubId hub_id;
    try {
        hub_id = ctx.hub_service.createHub(name, user_id);
    } catch (const std::exception& ex) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
            ex.what()));
        return out;
    } catch (...) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
            "Failed to create hub"));
        return out;
    }

    try {
        ctx.channel_service.createChannel(hub_id, "general", "text");
    } catch (...) {
    }

    ctx.subscription_manager.subscribeConnection(event->conn_id, Topic::HubTopic(hub_id));

    sercom::protocol::event::HubCreated created;
    created.mutable_hub()->set_id(hub_id.value);
    created.mutable_hub()->set_name(name);
    if (auto hub = ctx.hub_service.getHub(hub_id)) {
        created.mutable_hub()->mutable_metadata()->set_avatar_seed(hub->avatar_seed);
    }

    auto* self = created.mutable_self_member();
    self->set_hub_id(hub_id.value);
    self->set_user_id(user_id.value);
    self->set_is_online(true);
    self->set_role(to_proto_hub_role(ctx.hub_service.getMembershipRole(hub_id, user_id)));

    const auto channels = ctx.channel_service.getHubChannels(hub_id);
    if (!channels.empty()) {
        const auto& channel = channels.front();
        auto* ch = created.mutable_channel();
        ch->set_id(channel.id.value);
        ch->set_hub_id(hub_id.value);
        ch->set_type(to_proto_channel_type(channel.type));
        ch->mutable_metadata()->set_name(channel.name);
    }

    sercom::protocol::Envelope out_env;
    out_env.set_version(1);
    out_env.set_type(sercom::protocol::Envelope::HUB_CREATED);
    created.SerializeToString(out_env.mutable_payload());
    std::string bytes = out_env.SerializeAsString();

    out.emplace_back(net::outbound::OutgoingMessage{
        .target = net::outbound::Target::one(event->conn_id),
        .action =
            net::outbound::Action{std::in_place_type<net::outbound::SendPayload>,
                                  net::outbound::SendPayload{
                                      .payload = net::outbound::Payload{std::move(bytes), true}}}});
    return out;
}

}  // namespace app
