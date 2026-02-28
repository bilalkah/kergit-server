#include "app/commands/channel/DeleteChannelCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "domains/Channel.h"
#include "domains/Hub.h"
#include "utils/Metrics.h"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using nlohmann::json;

namespace app {

CommandResult DeleteChannelCommand::execute(CommandContext& ctx, const CommandInput cmd) {
    const auto* input = std::get_if<JsonInput>(&cmd);
    if (!input) {
        return std::unexpected(CommandError{1, "delete_channel expects JSON input"});
    }

    auto user_exp = ctx.session_manager.sessionOfConnection(input->conn);
    if (!user_exp.has_value()) {
        return std::unexpected(CommandError{2, "Authenticate first"});
    }
    const UserId user_id = user_exp.value();

    const auto j = json::parse(input->body, nullptr, false);
    if (j.is_discarded()) {
        return std::unexpected(CommandError{3, "Invalid JSON"});
    }

    const std::string channel_id_raw = j.value("channel_id", "");
    if (channel_id_raw.empty()) {
        return std::unexpected(CommandError{3, "channel_id is required"});
    }

    auto channel_id_opt = parse_wire_id<ChannelId>(channel_id_raw);
    if (!channel_id_opt.has_value()) {
        return std::unexpected(CommandError{4, "Channel not found"});
    }

    auto channel_opt = ctx.channel_service.getChannel(*channel_id_opt);
    if (!channel_opt.has_value()) {
        return std::unexpected(CommandError{5, "Channel not found"});
    }
    const Channel channel = channel_opt.value();

    auto role = ctx.hub_service.getMembershipRole(channel.hub_id, user_id);
    if (!role || (*role != Role::OWNER && *role != Role::ADMIN)) {
        return std::unexpected(CommandError{6, "Only admins/owners can delete channels"});
    }

    // Fan out before deleting to capture subscribers
    CommandSuccess res;
    json payload = {{"type", "channel_deleted"},
                    {"hub_id", channel.hub_id.value},
                    {"channel_id", channel.id.value}};

    utils::metrics::counters().fanout_subscriber_snapshot_total.fetch_add(
        1, std::memory_order_relaxed);
    auto subs = ctx.subscription_manager.getSubscribers(Topic::HubTopic(channel.hub_id));
    if (subs && !subs->empty()) {
        std::vector<GlobalConnId> conns;
        conns.reserve(subs->size());
        for (const auto& conn : *subs) {
            conns.push_back(conn);
        }
        if (!conns.empty()) {
            res.intents.push_back(Fanout{.conns = std::move(conns), .payload = payload});
        }
    }

    // Ack requester
    res.intents.push_back(Unicast{.conn = input->conn, .payload = payload});

    ctx.channel_service.deleteChannel(channel.id, channel.hub_id);
    ctx.subscription_manager.unsubscribeConnection(
        input->conn, Topic::ChannelTopic(channel.hub_id, channel.id));
    return res;
}

}  // namespace app
