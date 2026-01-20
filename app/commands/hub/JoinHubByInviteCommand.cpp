#include "app/commands/hub/JoinHubByInviteCommand.h"

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
#include "proto/event/session.pb.h"

#include <string>
#include <vector>

namespace app {

namespace {

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

bool parse_join_code(const std::string& join_code, uint64_t& out) {
    if (join_code.empty()) return false;
    try {
        std::size_t idx = 0;
        auto value = std::stoull(join_code, &idx, 10);
        if (idx != join_code.size()) return false;
        out = value;
        return true;
    } catch (...) {
        return false;
    }
}

}  // namespace

std::vector<net::outbound::OutgoingMessage> JoinHubByInviteCommand::execute(
    CommandContext& ctx, const queue::Event& evt) {
    const auto* event = std::get_if<queue::MessageEvent>(&evt);
    if (!event) {
        return {};
    }

    const auto& env = event->payload.env;
    if (env.type() != sercom::protocol::Envelope::HUB_JOIN) {
        return {};
    }

    sercom::protocol::command::JoinHub cmd;
    if (!cmd.ParseFromString(env.payload())) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INVALID_FORMAT,
                                   "Invalid HUB_JOIN payload")};
    }

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!user_exp.has_value()) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
                                   "Authenticate first")};
    }
    const UserId user_id = user_exp.value();

    uint64_t join_code = 0;
    if (!parse_join_code(cmd.join_code(), join_code)) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                                   "Join code is invalid")};
    }

    auto hub_id_opt = ctx.ids.to_internal(PublicHubId{join_code});
    if (!hub_id_opt.has_value()) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_NOT_FOUND,
                                   "Hub not found")};
    }
    const HubId hub_id = hub_id_opt.value();

    const auto snapshot = ctx.hub_service.getOrBuildSnapshot(hub_id);
    if (snapshot.name.empty()) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_NOT_FOUND,
                                   "Hub not found")};
    }

    const bool already_member = ctx.hub_service.isHubMember(hub_id, user_id);
    if (!already_member) {
        try {
            ctx.hub_service.addMember(hub_id, user_id, Role::USER);
        } catch (const std::exception& ex) {
            return {make_command_error(event->conn_id, env.type(),
                                       sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
                                       ex.what())};
        } catch (...) {
            return {make_command_error(event->conn_id, env.type(),
                                       sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
                                       "Failed to join hub")};
        }
    }

    ctx.subscription_manager.subscribe(user_id, Topic::HubTopic(hub_id));

    const auto public_hub_id = ctx.ids.to_public(hub_id).value;
    const auto public_user_id = ctx.ids.to_public(user_id).value;

    if (already_member) {
        auto user = ctx.user_service.getUser(user_id);
        sercom::protocol::event::HubAlreadyMember already;
        auto* member = already.mutable_self_member();
        member->set_hub_id(public_hub_id);
        member->set_user_id(public_user_id);
        member->set_is_online(true);
        member->set_role(role_to_proto(ctx.hub_service.getMembershipRole(hub_id, user_id)));
        member->set_display_name(resolve_display_name(ctx, user_id, ""));
        if (user) {
            member->set_avatar_seed(user->avatar_seed);
        }

        sercom::protocol::Envelope already_env;
        already_env.set_version(1);
        already_env.set_type(sercom::protocol::Envelope::HUB_ALREADY_MEMBER);
        already.SerializeToString(already_env.mutable_payload());

        std::string already_bytes;
        already_env.SerializeToString(&already_bytes);

        return {net::outbound::OutgoingMessage{
            .target = net::outbound::Target::one(event->conn_id),
            .action = net::outbound::SendPayload{
                .payload = net::outbound::Payload{.data = std::move(already_bytes),
                                                  .is_binary = true}}}};
    }

    sercom::protocol::event::SessionBootstrap bootstrap;
    auto* self = bootstrap.mutable_self();
    self->set_id(public_user_id);
    self->set_username(resolve_display_name(ctx, user_id, ""));
    if (auto user = ctx.user_service.getUser(user_id)) {
        self->set_avatar_seed(user->avatar_seed);
    }

    auto* hub_state = bootstrap.add_hubs();
    hub_state->mutable_hub()->set_id(public_hub_id);
    hub_state->mutable_hub()->set_name(snapshot.name);
    hub_state->mutable_hub()->set_avatar_seed(snapshot.avatar_seed);

    const auto channels = ctx.channel_service.getHubChannels(hub_id);
    for (const auto& channel : channels) {
        auto* ch = hub_state->add_channels();
        ch->set_id(ctx.ids.to_public(channel.id).value);
        ch->set_name(channel.name);
        ch->set_type(converters::to_proto_channel_type(channel.type));
    }

    const auto members = ctx.hub_service.getHubMembers(hub_id);
    for (const auto& member : members) {
        const auto& member_id = member.user_id;
        auto* m = hub_state->add_members();
        m->set_hub_id(public_hub_id);
        m->set_user_id(ctx.ids.to_public(member_id).value);
        m->set_is_online(ctx.presence_manager.isUserOnline(member_id) || member_id == user_id);
        m->set_role(role_to_proto(ctx.hub_service.getMembershipRole(hub_id, member_id)));
        m->set_display_name(resolve_display_name(ctx, member_id, member.display_name));
        m->set_avatar_seed(member.avatar_seed);
    }

    sercom::protocol::Envelope joined_env;
    joined_env.set_version(1);
    joined_env.set_type(sercom::protocol::Envelope::SESSION_BOOTSTRAP);
    bootstrap.SerializeToString(joined_env.mutable_payload());

    std::string joined_bytes;
    joined_env.SerializeToString(&joined_bytes);

    std::vector<net::outbound::OutgoingMessage> out;
    out.push_back(net::outbound::OutgoingMessage{
        .target = net::outbound::Target::one(event->conn_id),
        .action = net::outbound::SendPayload{
            .payload = net::outbound::Payload{.data = std::move(joined_bytes), .is_binary = true}}});

    if (!already_member) {
        auto subs = ctx.subscription_manager.getSubscribers(Topic::HubTopic(hub_id));
        if (subs.has_value()) {
            std::vector<GlobalConnId> conns;
            conns.reserve(subs->size());
            for (const auto& uid : subs.value()) {
                if (uid == user_id) continue;
                auto conn = ctx.session_manager.getMainConnection(uid);
                if (conn.has_value()) conns.push_back(conn.value());
            }
            if (!conns.empty()) {
                sercom::protocol::event::HubMemberJoined member_joined;
                auto* member = member_joined.mutable_member();
                member->set_hub_id(public_hub_id);
                member->set_user_id(public_user_id);
                member->set_is_online(true);
                member->set_role(role_to_proto(ctx.hub_service.getMembershipRole(hub_id, user_id)));
                member->set_display_name(resolve_display_name(ctx, user_id, ""));
                if (auto user = ctx.user_service.getUser(user_id)) {
                    member->set_avatar_seed(user->avatar_seed);
                }

                sercom::protocol::Envelope env_out;
                env_out.set_version(1);
                env_out.set_type(sercom::protocol::Envelope::HUB_MEMBER_JOINED);
                member_joined.SerializeToString(env_out.mutable_payload());

                std::string bytes;
                env_out.SerializeToString(&bytes);

                out.push_back(net::outbound::OutgoingMessage{
                    .target = net::outbound::Target::many(std::move(conns)),
                    .action = net::outbound::SendPayload{
                        .payload =
                            net::outbound::Payload{.data = std::move(bytes), .is_binary = true}}});
            }
        }
    }

    return out;
}

}  // namespace app
