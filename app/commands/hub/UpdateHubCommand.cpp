#include "app/commands/hub/UpdateHubCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "domains/Hub.h"
#include "proto/command/hub.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"
#include "proto/event/hub.pb.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <vector>

namespace app {

std::string UpdateHubCommand::sanitize(std::string value) {
    auto trim = [](std::string& s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                                        [](unsigned char ch) { return !std::isspace(ch); }));
        s.erase(
            std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); })
                .base(),
            s.end());
    };
    trim(value);
    if (value.size() > 64) value.resize(64);
    return value;
}

std::vector<net::outbound::OutgoingMessage> UpdateHubCommand::execute(CommandContext& ctx,
                                                                      const queue::Event& evt) {
    const auto* event = std::get_if<queue::MessageEvent>(&evt);
    if (!event) {
        return {};
    }

    const auto& env = event->payload.env;
    if (env.type() != sercom::protocol::Envelope::HUB_UPDATE) {
        return {};
    }

    const auto* cmd = get_parsed<sercom::protocol::command::UpdateHub>(*event);
    if (!cmd) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INVALID_FORMAT,
                                   "Invalid HUB_UPDATE payload"));
    }

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!user_exp.has_value()) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
                                   "Authenticate first"));
    }
    const UserId user_id = user_exp.value();

    std::optional<std::string> requested_name;
    std::optional<std::string> requested_seed;

    for (int i = 0; i < cmd->changes_size(); ++i) {
        const auto& change = cmd->changes(i);
        switch (change.change_case()) {
            case sercom::protocol::command::HubChange::kName: {
                auto name = sanitize(change.name());
                if (name.empty()) {
                    return single_outgoing(make_command_error(event->conn_id, env.type(),
                                               sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                                               "Hub name is required"));
                }
                requested_name = std::move(name);
                break;
            }
            case sercom::protocol::command::HubChange::kAvatarSeed: {
                auto seed = sanitize(change.avatar_seed());
                if (seed.empty()) {
                    return single_outgoing(make_command_error(event->conn_id, env.type(),
                                               sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                                               "Avatar seed is required"));
                }
                requested_seed = std::move(seed);
                break;
            }
            case sercom::protocol::command::HubChange::CHANGE_NOT_SET:
            default:
                return single_outgoing(make_command_error(event->conn_id, env.type(),
                                           sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                                           "Invalid change type"));
        }
    }

    if (!requested_name && !requested_seed) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                                   "No changes requested"));
    }

    auto hub_id_opt = ctx.ids.to_internal(PublicHubId{cmd->hub_id()});
    if (!hub_id_opt.has_value()) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_NOT_FOUND,
                                   "Hub not found"));
    }
    const HubId hub_id = hub_id_opt.value();

    if (!ctx.hub_service.isHubMember(hub_id, user_id)) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                   "Join the hub before updating it"));
    }

    auto role = ctx.hub_service.getMembershipRole(hub_id, user_id);
    if (!role || (*role != Role::OWNER && *role != Role::ADMIN)) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                   "Only admins or owners can update hub settings"));
    }

    try {
        if (requested_name) {
            if (!ctx.hub_service.renameHub(hub_id, *requested_name)) {
                return single_outgoing(make_command_error(event->conn_id, env.type(),
                                           sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
                                           "Unable to rename hub at this time"));
            }
        }
        if (requested_seed) {
            if (!ctx.hub_service.updateHubAvatarSeed(hub_id, *requested_seed)) {
                return single_outgoing(make_command_error(event->conn_id, env.type(),
                                           sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
                                           "Unable to update hub avatar at this time"));
            }
        }
    } catch (const std::exception& ex) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
                                   ex.what()));
    } catch (...) {
        return single_outgoing(make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
                                   "Unable to update hub settings"));
    }

    std::vector<GlobalConnId> conns;
    utils::metrics::counters().fanout_subscriber_snapshot_total.fetch_add(
        1, std::memory_order_relaxed);
    auto subs = ctx.subscription_manager.getSubscribers(Topic::HubTopic(hub_id));
    if (subs) {
        conns.reserve(subs->size());
        for (const auto& conn : *subs) {
            conns.push_back(conn);
        }
    }

    if (conns.empty()) {
        return {};
    }

    std::vector<net::outbound::OutgoingMessage> out;
    if (requested_name) {
        sercom::protocol::event::HubRenamed renamed;
        renamed.set_hub_id(ctx.ids.to_public(hub_id).value);
        renamed.set_name(*requested_name);

        sercom::protocol::Envelope out_env;
        out_env.set_version(1);
        out_env.set_type(sercom::protocol::Envelope::HUB_RENAMED);
        renamed.SerializeToString(out_env.mutable_payload());

        std::string bytes;
        out_env.SerializeToString(&bytes);

        out.push_back(net::outbound::OutgoingMessage{
            .target = net::outbound::Target::many(conns),
            .action = net::outbound::Action{
                std::in_place_type<net::outbound::SendPayload>,
                net::outbound::SendPayload{.payload = net::outbound::Payload{std::move(bytes), true}}}});
    }

    if (requested_seed) {
        sercom::protocol::event::HubAvatarChanged changed;
        changed.set_hub_id(ctx.ids.to_public(hub_id).value);
        changed.set_avatar_seed(*requested_seed);

        sercom::protocol::Envelope out_env;
        out_env.set_version(1);
        out_env.set_type(sercom::protocol::Envelope::HUB_AVATAR_CHANGED);
        changed.SerializeToString(out_env.mutable_payload());

        std::string bytes;
        out_env.SerializeToString(&bytes);

        out.push_back(net::outbound::OutgoingMessage{
            .target = net::outbound::Target::many(conns),
            .action = net::outbound::Action{
                std::in_place_type<net::outbound::SendPayload>,
                net::outbound::SendPayload{.payload = net::outbound::Payload{std::move(bytes), true}}}});
    }

    return out;
}

}  // namespace app
