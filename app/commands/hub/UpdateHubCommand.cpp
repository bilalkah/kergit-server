#include "app/commands/hub/UpdateHubCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "domains/Hub.h"
#include "proto/command/hub.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"
#include "proto/event/state.pb.h"

#include <cassert>
#include <optional>
#include <string>
#include <vector>

namespace app {

std::vector<net::outbound::OutgoingMessage> UpdateHubCommand::execute(CommandContext& ctx,
                                                                      const queue::Event& evt) {
    std::vector<net::outbound::OutgoingMessage> out;
    assert(std::holds_alternative<queue::MessageEvent>(evt));
    const auto* event = &std::get<queue::MessageEvent>(evt);
    const auto& env = event->payload.env;
    assert(env.type() == sercom::protocol::Envelope::HUB_UPDATE);

    const auto& cmd = require_parsed<sercom::protocol::command::UpdateHub>(*event);

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!user_exp.has_value()) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
            "Authenticate first"));
        return out;
    }
    const UserId user_id = user_exp.value();

    std::optional<std::string> requested_name;
    std::optional<std::string> requested_seed;

    if (cmd.has_name()) {
        auto name = sanitize(cmd.name());
        if (name.size() > 64) name.resize(64);
        if (name.empty()) {
            out.emplace_back(make_command_error(
                event->conn_id, env.type(),
                sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                "Hub name is required"));
            return out;
        }
        requested_name = std::move(name);
    }

    if (cmd.has_avatar_seed()) {
        auto seed = sanitize(cmd.avatar_seed());
        if (seed.size() > 64) seed.resize(64);
        if (seed.empty()) {
            out.emplace_back(make_command_error(
                event->conn_id, env.type(),
                sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                "Avatar seed is required"));
            return out;
        }
        requested_seed = std::move(seed);
    }

    if (!requested_name && !requested_seed) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "No changes requested"));
        return out;
    }

    const HubId hub_id{cmd.hub_id()};
    auto role = ctx.hub_service.getMembershipRole(hub_id, user_id);
    if (!role) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_FORBIDDEN,
            "Join the hub before updating it"));
        return out;
    }

    if (*role != Role::OWNER && *role != Role::ADMIN) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_FORBIDDEN,
            "Only admins or owners can update hub settings"));
        return out;
    }

    try {
        if (requested_name) {
            if (!ctx.hub_service.renameHub(hub_id, *requested_name)) {
                out.emplace_back(
                    make_command_error(event->conn_id, env.type(),
                                       sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
                                       "Unable to rename hub at this time"));
                return out;
            }
        }
        if (requested_seed) {
            if (!ctx.hub_service.updateHubAvatarSeed(hub_id, *requested_seed)) {
                out.emplace_back(
                    make_command_error(event->conn_id, env.type(),
                                       sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
                                       "Unable to update hub avatar at this time"));
                return out;
            }
        }
    } catch (const std::exception& ex) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
            ex.what()));
        return out;
    } catch (...) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
            "Unable to update hub settings"));
        return out;
    }

    // Fetch the updated hub to get final state
    auto updated_hub = ctx.hub_service.getHub(hub_id);
    if (!updated_hub) {
        return {};
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

    sercom::protocol::event::StateDelta delta;
    auto* hub_delta = delta.add_hubs();
    hub_delta->set_hub_id(hub_id.value);
    auto* upsert = hub_delta->add_hub_ops()->mutable_upsert()->mutable_hub();
    upsert->set_id(hub_id.value);
    upsert->set_name(updated_hub->name);
    upsert->mutable_metadata()->set_avatar_seed(updated_hub->avatar_seed);

    out.emplace_back(make_outgoing_message(net::outbound::Target::many(std::move(conns)),
                                           make_state_delta(delta)));
    return out;
}

}  // namespace app
