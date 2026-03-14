#include "app/commands/hub/JoinHubByInviteCommand.h"

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
#include "proto/event/session.pb.h"

#include <cassert>
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

}  // namespace

std::vector<net::outbound::OutgoingMessage> JoinHubByInviteCommand::execute(
    CommandContext& ctx, const queue::Event& evt) {
    std::vector<net::outbound::OutgoingMessage> out;
    assert(std::holds_alternative<queue::MessageEvent>(evt));
    const auto* event = &std::get<queue::MessageEvent>(evt);
    const auto& env = event->payload.env;
    assert(env.type() == sercom::protocol::Envelope::HUB_JOIN);

    const auto& cmd = require_parsed<sercom::protocol::command::JoinHub>(*event);

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!user_exp.has_value()) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
            "Authenticate first"));
        return out;
    }
    const UserId user_id = user_exp.value();

    std::string raw_code = cmd.join_code();
    auto invite_pos = raw_code.find("/invite/");
    std::string token =
        (invite_pos != std::string::npos) ? raw_code.substr(invite_pos + 8) : raw_code;

    auto hub_id_opt = ctx.invite_service.resolveInvite(token);
    if (!hub_id_opt.has_value()) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVITE_EXPIRED,
            "Invite link is invalid or has expired"));
        return out;
    }
    const HubId hub_id = hub_id_opt.value();

    const auto snapshot = ctx.hub_service.getOrBuildSnapshot(hub_id);
    if (snapshot.name.empty()) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_NOT_FOUND,
            "Hub not found"));
        return out;
    }

    const bool already_member = ctx.hub_service.isHubMember(hub_id, user_id);
    if (!already_member) {
        try {
            ctx.hub_service.addMember(hub_id, user_id, Role::USER);
        } catch (const std::exception& ex) {
            out.emplace_back(make_command_error(
                event->conn_id, env.type(),
                sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR, ex.what()));
            return out;
        } catch (...) {
            out.emplace_back(make_command_error(
                event->conn_id, env.type(),
                sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR, "Failed to join hub"));
            return out;
        }
    }

    ctx.subscription_manager.subscribeConnection(event->conn_id, Topic::HubTopic(hub_id));

    if (already_member) {
        sercom::protocol::event::HubAlreadyMember already;
        auto* member = already.mutable_self_member();
        member->set_hub_id(hub_id.value);
        member->set_user_id(user_id.value);
        member->set_is_online(true);
        member->set_role(to_proto_hub_role(ctx.hub_service.getMembershipRole(hub_id, user_id)));

        sercom::protocol::Envelope already_env;
        already_env.set_version(1);
        already_env.set_type(sercom::protocol::Envelope::HUB_ALREADY_MEMBER);
        already.SerializeToString(already_env.mutable_payload());
        std::string already_bytes = already_env.SerializeAsString();

        out.emplace_back(net::outbound::OutgoingMessage{
            .target = net::outbound::Target::one(event->conn_id),
            .action = net::outbound::Action{
                std::in_place_type<net::outbound::SendPayload>,
                net::outbound::SendPayload{
                    .payload = net::outbound::Payload{std::move(already_bytes), true}}}});
        return out;
    }

    sercom::protocol::event::SessionBootstrap bootstrap;
    auto* self = bootstrap.mutable_self();
    self->set_id(user_id.value);
    {
        auto display = resolve_display_name(ctx, user_id, "");
        self->mutable_metadata()->set_username(display);
        if (auto user = ctx.user_service.getUser(user_id)) {
            self->mutable_metadata()->set_avatar_seed(user->avatar_seed);
        }
    }

    auto* hub_state = bootstrap.add_hubs();
    hub_state->mutable_hub()->set_id(hub_id.value);
    hub_state->mutable_hub()->set_name(snapshot.name);
    hub_state->mutable_hub()->mutable_metadata()->set_avatar_seed(snapshot.avatar_seed);

    const auto channels = ctx.channel_service.getHubChannels(hub_id);
    for (const auto& channel : channels) {
        auto* ch = hub_state->add_channels();
        ch->set_id(channel.id.value);
        ch->set_hub_id(hub_id.value);
        ch->set_type(to_proto_channel_type(channel.type));
        ch->mutable_metadata()->set_name(channel.name);
    }

    const auto members = ctx.hub_service.getHubMembers(hub_id);
    for (const auto& member : members) {
        const auto& member_id = member.user_id;
        auto* m = hub_state->add_members();
        m->set_hub_id(hub_id.value);
        m->set_user_id(member_id.value);
        m->set_is_online(ctx.presence_manager.isUserOnline(member_id) || member_id == user_id);
        m->set_role(to_proto_hub_role(ctx.hub_service.getMembershipRole(hub_id, member_id)));

        // Add user to the users lookup table
        if (auto user_info = ctx.user_service.getUser(member_id)) {
            auto* u = hub_state->add_users();
            u->set_id(member_id.value);
            u->mutable_metadata()->set_username(
                resolve_display_name(ctx, member_id, member.display_name));
            u->mutable_metadata()->set_avatar_seed(user_info->avatar_seed);
        }
    }

    sercom::protocol::Envelope joined_env;
    joined_env.set_version(1);
    joined_env.set_type(sercom::protocol::Envelope::SESSION_BOOTSTRAP);
    bootstrap.SerializeToString(joined_env.mutable_payload());
    std::string joined_bytes = joined_env.SerializeAsString();

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
                auto display = resolve_display_name(ctx, user_id, "");
                std::string avatar;
                if (auto user = ctx.user_service.getUser(user_id)) {
                    avatar = user->avatar_seed;
                }
                std::string bytes = ctx.hub_notifier.memberJoined(
                    hub_id, user_id,
                    ctx.hub_service.getMembershipRole(hub_id, user_id).value_or(Role::USER),
                    display, avatar, true);

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
