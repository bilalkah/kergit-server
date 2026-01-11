#include "app/commands/hub/LeaveHubCommand.h"

#include "app/commands/CommandJson.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "domains/Hub.h"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using nlohmann::json;

namespace app {

CommandResult LeaveHubCommand::execute(CommandContext& ctx, const CommandInput cmd) {
    const auto* input = std::get_if<JsonInput>(&cmd);
    if (!input) {
        return std::unexpected(CommandError{"invalid_input", "leave_hub expects JSON input"});
    }

    auto user_exp = ctx.session_manager.sessionOfConnection(input->conn);
    if (!user_exp.has_value()) {
        return std::unexpected(CommandError{"not_authenticated", "Authenticate first"});
    }
    const UserId user_id = user_exp.value();

    auto hub_raw = commands::read_uint64(input->body, "hub_id");
    if (!hub_raw.has_value()) {
        return std::unexpected(CommandError{"missing_hub_id", "hub_id is required"});
    }

    auto hub_id_opt = ctx.ids.to_internal(PublicHubId{hub_raw.value()});
    if (!hub_id_opt.has_value()) {
        return std::unexpected(CommandError{"hub_not_found", "Hub not found"});
    }
    const HubId hub_id = hub_id_opt.value();

    auto role = ctx.hub_service.getMembershipRole(hub_id, user_id);
    if (!role.has_value()) {
        return std::unexpected(CommandError{"not_in_hub", "Join the hub before leaving it"});
    }
    if (*role == Role::OWNER) {
        return std::unexpected(CommandError{"hub_owner_must_transfer",
                                            "Owners must transfer ownership before leaving"});
    }

    // Remove membership
    if (!ctx.hub_service.isHubMember(hub_id, user_id)) {
        return std::unexpected(CommandError{"not_in_hub", "Join the hub before leaving it"});
    }
    ctx.hub_service.removeMember(hub_id, user_id);

    // Unsubscribe from hub + channels
    ctx.subscription_manager.unsubscribe(user_id, Topic::HubTopic(hub_id));
    const auto channels = ctx.channel_service.getHubChannels(hub_id);
    for (const auto& ch : channels) {
        ctx.subscription_manager.unsubscribe(user_id, Topic::ChannelTopic(hub_id, ch.id));
    }

    const auto public_hub_id = ctx.ids.to_public(hub_id);
    const auto public_user_id = ctx.ids.to_public(user_id);
    json payload = {{"type", "hub_left"}, {"hub_id", public_hub_id.value}};

    CommandSuccess res;
    // Ack requester
    res.intents.push_back(Unicast{.conn = input->conn, .payload = payload});

    // Notify hub members that this user left (remove from roster)
    auto hub_subs = ctx.subscription_manager.getSubscribers(Topic::HubTopic(hub_id));
    if (hub_subs.has_value() && !hub_subs->empty()) {
        std::vector<GlobalConnId> conns;
        conns.reserve(hub_subs->size());
        for (const auto& uid : hub_subs.value()) {
            if (uid == user_id) continue;
            auto c = ctx.session_manager.getMainConnection(uid);
            if (c.has_value()) conns.push_back(c.value());
        }
        if (!conns.empty()) {
            json offline = {{"type", "member_left"},
                            {"hub_id", public_hub_id.value},
                            {"user_id", public_user_id.value}};
            res.intents.push_back(Fanout{.conns = conns, .payload = offline});

            // Backward/compat: also emit member_offline so clients that only handle presence still update
            json offline_presence = {{"type", "member_offline"},
                                     {"hub_id", public_hub_id.value},
                                     {"user_id", public_user_id.value}};
            res.intents.push_back(Fanout{.conns = std::move(conns), .payload = std::move(offline_presence)});
        }
    }

    return res;
}

}  // namespace app
