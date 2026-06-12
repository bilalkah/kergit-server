#include "app/commands/hub/KickHubMemberCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "app/services/hub/RolePolicy.h"
#include "proto/command/hub.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"
#include "proto/event/state.pb.h"

#include <cassert>
#include <exception>
#include <vector>

namespace app {

std::vector<net::outbound::OutgoingMessage> KickHubMemberCommand::execute(CommandContext& ctx,
                                                                          const queue::Event& evt) {
    std::vector<net::outbound::OutgoingMessage> out;
    assert(std::holds_alternative<queue::MessageEvent>(evt));
    const auto* event = &std::get<queue::MessageEvent>(evt);
    const auto& env = event->payload.env;
    assert(env.type() == sercom::protocol::Envelope::HUB_MEMBER_KICK);

    const auto& cmd = require_parsed<sercom::protocol::command::KickHubMember>(*event);

    auto actor_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!actor_exp.has_value()) {
        out.emplace_back(make_command_error(event->conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
                                            "Authenticate first"));
        return out;
    }
    const UserId actor_id = actor_exp.value();

    const HubId hub_id{cmd.hub_id()};
    const UserId target_id{cmd.user_id()};
    if (hub_id.value.empty() || target_id.value.empty()) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "hub_id and user_id are required"));
        return out;
    }

    auto actor_role = ctx.hub_service.getMembershipRole(hub_id, actor_id);
    if (!actor_role.has_value()) {
        out.emplace_back(make_command_error(event->conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                            "Join the hub before moderating members"));
        return out;
    }

    auto target_role = ctx.hub_service.getMembershipRole(hub_id, target_id);
    if (!target_role.has_value()) {
        out.emplace_back(make_command_error(event->conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_NOT_FOUND,
                                            "Target user is not a member"));
        return out;
    }

    if (!services::hub::canKickHubMember(*actor_role, *target_role, actor_id, target_id)) {
        out.emplace_back(make_command_error(event->conn_id, env.type(),
                                            sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                            "You are not allowed to kick this member"));
        return out;
    }

    if (const auto target_channel = ctx.voice_service.sessions().user_channel(target_id);
        target_channel.has_value()) {
        const auto channel = ctx.hub_service.getChannel(*target_channel);
        if (channel.has_value() && channel->hub_id == hub_id) {
            if (!ctx.voice_service.verified_kick_user(*target_channel, target_id)) {
                out.emplace_back(
                    make_command_error(event->conn_id, env.type(),
                                       sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
                                       "Failed to remove user from voice channel"));
                return out;
            }
        }
    }

    try {
        ctx.hub_service.removeMember(hub_id, target_id);
    } catch (const std::exception&) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
            "Failed to kick member"));
        return out;
    } catch (...) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
            "Failed to kick member"));
        return out;
    }

    // Membership is now removed; invalidate the hub's active invite so links
    // shared before the kick can no longer be used to rejoin. Best-effort: the
    // membership removal already committed and cannot be rolled back, so a Redis
    // revocation failure is logged rather than failing the kick.
    try {
        ctx.invite_service.revokeInvitesForHub(hub_id);
    } catch (const std::exception& ex) {
        utils::log_line(utils::LogLevel::WARN,
                        std::string("KickHubMemberCommand: invite revocation failed for hub_id=") +
                            hub_id.value + " error=" + ex.what());
    } catch (...) {
        utils::log_line(utils::LogLevel::WARN,
                        std::string("KickHubMemberCommand: invite revocation failed for hub_id=") +
                            hub_id.value);
    }

    const auto target_conns = ctx.session_manager.getSessionConnections(target_id);
    const auto channel_ids = ctx.hub_service.getHubChannelIds(hub_id);
    for (const auto& conn : target_conns) {
        ctx.subscription_manager.unsubscribeConnection(conn, Topic::HubTopic(hub_id));
        for (const auto& channel_id : channel_ids) {
            ctx.subscription_manager.unsubscribeConnection(conn,
                                                           Topic::ChannelTopic(hub_id, channel_id));
        }
    }

    utils::metrics::counters().fanout_subscriber_snapshot_total.fetch_add(
        1, std::memory_order_relaxed);
    const auto subs = ctx.subscription_manager.getSubscribers(Topic::HubTopic(hub_id));
    if (subs && !subs->empty()) {
        sercom::protocol::event::StateDelta delta;
        auto* hub_delta = delta.add_hubs();
        hub_delta->set_hub_id(hub_id.value);
        hub_delta->add_member_ops()->mutable_remove()->set_user_id(target_id.value);
        /*
         * Existing invite links for this hub were revoked after this kick.
         * Empty join_code means clients must clear their cached invite link.
         */
        hub_delta->add_hub_ops()->mutable_join_code_set()->set_join_code("");

        std::vector<GlobalConnId> recipients{subs->begin(), subs->end()};
        out.emplace_back(make_outgoing_message(net::outbound::Target::many(std::move(recipients)),
                                               make_state_delta(delta)));
    }

    if (!target_conns.empty()) {
        sercom::protocol::event::StateDelta self_delta;
        auto* hub_delta = self_delta.add_hubs();
        hub_delta->set_hub_id(hub_id.value);
        hub_delta->add_hub_ops()->mutable_remove();
        out.emplace_back(make_outgoing_message(net::outbound::Target::many(target_conns),
                                               make_state_delta(self_delta)));
    }

    return out;
}

}  // namespace app
