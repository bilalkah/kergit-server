#include "app/commands/hub/JoinHubByInviteCommand.h"

#include "app/commands/utils.h"
#include "app/converters/ProtoConverters.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "app/proto_builders/EnvelopeBuilders.h"
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
    if (!role) return sercom::protocol::domain::HubRole_MEMBER;
    return converters::to_proto_hub_role(*role);
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

    const auto& cmd = require_parsed<sercom::protocol::command::JoinHub>(*event);

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!user_exp.has_value()) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
            "Authenticate first"));
    }
    const UserId user_id = user_exp.value();

    std::string raw_code = cmd.join_code();
    auto invite_pos = raw_code.find("/invite/");
    std::string token = (invite_pos != std::string::npos) ? raw_code.substr(invite_pos + 8)
                                                          : raw_code;

    auto hub_id_opt = ctx.invite_service.resolveInvite(token);
    if (!hub_id_opt.has_value()) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVITE_EXPIRED,
            "Invite link is invalid or has expired"));
    }
    const HubId hub_id = hub_id_opt.value();

    const auto snapshot = ctx.hub_service.getOrBuildSnapshot(hub_id);
    if (snapshot.name.empty()) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_NOT_FOUND,
            "Hub not found"));
    }

    const bool already_member = ctx.hub_service.isHubMember(hub_id, user_id);
    if (!already_member) {
        try {
            ctx.hub_service.addMember(hub_id, user_id, Role::USER);
        } catch (const std::exception& ex) {
            return single_outgoing(make_command_error(
                event->conn_id, env.type(),
                sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR, ex.what()));
        } catch (...) {
            return single_outgoing(make_command_error(
                event->conn_id, env.type(),
                sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR, "Failed to join hub"));
        }
    }

    ctx.subscription_manager.subscribeConnection(event->conn_id, Topic::HubTopic(hub_id));

    if (already_member) {
        auto user = ctx.user_service.getUser(user_id);
        sercom::protocol::event::HubAlreadyMember already;
        auto* member = already.mutable_self_member();
        member->set_hub_id(hub_id.value);
        member->set_user_id(user_id.value);
        member->set_is_online(true);
        member->set_role(role_to_proto(ctx.hub_service.getMembershipRole(hub_id, user_id)));
        member->set_display_name(resolve_display_name(ctx, user_id, ""));
        if (user) {
            member->set_avatar_seed(user->avatar_seed);
        }

        std::string already_bytes = proto_builders::serialize_envelope(
            sercom::protocol::Envelope::HUB_ALREADY_MEMBER, already);

        return single_outgoing(net::outbound::OutgoingMessage{
            .target = net::outbound::Target::one(event->conn_id),
            .action = net::outbound::Action{
                std::in_place_type<net::outbound::SendPayload>,
                net::outbound::SendPayload{
                    .payload = net::outbound::Payload{std::move(already_bytes), true}}}});
    }

    sercom::protocol::event::SessionBootstrap bootstrap;
    auto* self = bootstrap.mutable_self();
    self->set_id(user_id.value);
    self->set_username(resolve_display_name(ctx, user_id, ""));
    if (auto user = ctx.user_service.getUser(user_id)) {
        self->set_avatar_seed(user->avatar_seed);
    }

    auto* hub_state = bootstrap.add_hubs();
    hub_state->mutable_hub()->set_id(hub_id.value);
    hub_state->mutable_hub()->set_name(snapshot.name);
    hub_state->mutable_hub()->set_avatar_seed(snapshot.avatar_seed);

    const auto channels = ctx.channel_service.getHubChannels(hub_id);
    for (const auto& channel : channels) {
        auto* ch = hub_state->add_channels();
        ch->set_id(channel.id.value);
        ch->set_name(channel.name);
        ch->set_type(converters::to_proto_channel_type(channel.type));
    }

    const auto members = ctx.hub_service.getHubMembers(hub_id);
    for (const auto& member : members) {
        const auto& member_id = member.user_id;
        auto* m = hub_state->add_members();
        m->set_hub_id(hub_id.value);
        m->set_user_id(member_id.value);
        m->set_is_online(ctx.presence_manager.isUserOnline(member_id) || member_id == user_id);
        m->set_role(role_to_proto(ctx.hub_service.getMembershipRole(hub_id, member_id)));
        m->set_display_name(resolve_display_name(ctx, member_id, member.display_name));
        m->set_avatar_seed(member.avatar_seed);
    }

    std::string joined_bytes = proto_builders::serialize_envelope(
        sercom::protocol::Envelope::SESSION_BOOTSTRAP, bootstrap);

    std::vector<net::outbound::OutgoingMessage> out;
    out.emplace_back();
    auto& joined_msg = out.back();
    joined_msg.target = net::outbound::Target::one(event->conn_id);
    joined_msg.action.emplace<net::outbound::SendPayload>(net::outbound::SendPayload{
        .payload = net::outbound::Payload{std::move(joined_bytes), true}});

    if (!already_member) {
        utils::metrics::counters().fanout_subscriber_snapshot_total.fetch_add(
            1, std::memory_order_relaxed);
        auto subs = ctx.subscription_manager.getSubscribers(Topic::HubTopic(hub_id));
        if (subs) {
            std::vector<GlobalConnId> conns;
            conns.reserve(subs->size());
            for (const auto& conn : *subs) {
                if (conn == event->conn_id) continue;
                conns.push_back(conn);
            }
            if (!conns.empty()) {
                sercom::protocol::event::HubMemberJoined member_joined;
                auto* member = member_joined.mutable_member();
                member->set_hub_id(hub_id.value);
                member->set_user_id(user_id.value);
                member->set_is_online(true);
                member->set_role(role_to_proto(ctx.hub_service.getMembershipRole(hub_id, user_id)));
                member->set_display_name(resolve_display_name(ctx, user_id, ""));
                if (auto user = ctx.user_service.getUser(user_id)) {
                    member->set_avatar_seed(user->avatar_seed);
                }

                std::string bytes = proto_builders::serialize_envelope(
                    sercom::protocol::Envelope::HUB_MEMBER_JOINED, member_joined);

                out.emplace_back();
                auto& member_msg = out.back();
                member_msg.target = net::outbound::Target::many(std::move(conns));
                member_msg.action.emplace<net::outbound::SendPayload>(net::outbound::SendPayload{
                    .payload = net::outbound::Payload{std::move(bytes), true}});
            }
        }
    }

    return out;
}

}  // namespace app
