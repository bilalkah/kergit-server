#include "app/commands/hub/DeleteHubCommand.h"

#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "domains/Channel.h"
#include "domains/Hub.h"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using nlohmann::json;

namespace app {

CommandResult DeleteHubCommand::execute(CommandContext& ctx, const CommandInput cmd) {
    const auto* input = std::get_if<JsonInput>(&cmd);
    if (!input) {
        return std::unexpected(CommandError{"invalid_input", "delete_hub expects JSON input"});
    }

    auto user_exp = ctx.session_manager.sessionOfConnection(input->conn);
    if (!user_exp.has_value()) {
        return std::unexpected(CommandError{"not_authenticated", "Authenticate first"});
    }
    const UserId user_id = user_exp.value();

    const std::string hub_raw = input->body.value("hub_id", "");
    if (hub_raw.empty()) {
        return std::unexpected(CommandError{"missing_hub_id", "hub_id is required"});
    }

    auto hub_id_opt = ctx.ids.to_internal(PublicHubId{hub_raw});
    if (!hub_id_opt.has_value()) {
        return std::unexpected(CommandError{"hub_not_found", "Hub not found"});
    }
    const HubId hub_id = hub_id_opt.value();

    if (!ctx.hub_service.isHubMember(hub_id, user_id)) {
        return std::unexpected(CommandError{"not_in_hub", "Join the hub before deleting it"});
    }

    auto role = ctx.hub_service.getMembershipRole(hub_id, user_id);
    if (!role.has_value() || *role != Role::OWNER) {
        return std::unexpected(
            CommandError{"insufficient_privilege", "Only owners can delete hubs"});
    }

    const auto channels = ctx.channel_service.getHubChannels(hub_id);

    if (!ctx.hub_service.deleteHub(hub_id, user_id)) {
        return std::unexpected(
            CommandError{"delete_hub_failed", "Unable to delete hub at this time"});
    }

    const auto public_hub_id = ctx.ids.to_public(hub_id);
    json payload = {{"type", "hub_deleted"}, {"hub_id", public_hub_id.value}};

    CommandSuccess res;

    // Inform channel subscribers the hub (and channels) are gone
    for (const auto& ch : channels) {
        json channel_closed = {{"type", "channel_closed"},
                               {"channel_id", ctx.ids.to_public(ch.id).value},
                               {"reason", "hub_deleted"}};
        auto channel_subs =
            ctx.subscription_manager.getSubscribers(Topic::ChannelTopic(hub_id, ch.id));
        if (channel_subs.has_value() && !channel_subs->empty()) {
            std::vector<GlobalConnId> conns;
            conns.reserve(channel_subs->size());
            for (const auto& uid : channel_subs.value()) {
                auto conn = ctx.session_manager.getMainConnection(uid);
                if (conn.has_value()) conns.push_back(conn.value());
            }
            if (!conns.empty()) {
                res.intents.push_back(Fanout{.conns = std::move(conns),
                                             .payload = channel_closed});
            }
        }
    }

    // Broadcast hub deletion to hub subscribers
    auto subs = ctx.subscription_manager.getSubscribers(Topic::HubTopic(hub_id));
    if (subs.has_value() && !subs->empty()) {
        std::vector<GlobalConnId> conns;
        conns.reserve(subs->size());
        for (const auto& uid : subs.value()) {
            auto conn = ctx.session_manager.getMainConnection(uid);
            if (conn.has_value()) conns.push_back(conn.value());
        }
        if (!conns.empty()) {
            res.intents.push_back(Fanout{.conns = std::move(conns), .payload = payload});
        }
    }

    // Ack requester
    res.intents.push_back(Unicast{.conn = input->conn, .payload = std::move(payload)});

    // Unsubscribe owner from hub and its channels locally
    ctx.subscription_manager.unsubscribe(user_id, Topic::HubTopic(hub_id));
    for (const auto& ch : channels) {
        ctx.subscription_manager.unsubscribe(user_id, Topic::ChannelTopic(hub_id, ch.id));
    }

    return res;
}

}  // namespace app
