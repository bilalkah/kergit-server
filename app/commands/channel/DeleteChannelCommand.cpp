#include "app/commands/channel/DeleteChannelCommand.h"

#include "app/commands/CommandJson.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "domains/Channel.h"
#include "domains/Hub.h"

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

    auto channel_id_raw = commands::read_uint64(input->body, "channel_id");
    if (!channel_id_raw.has_value()) {
        return std::unexpected(CommandError{3, "channel_id is required"});
    }

    auto channel_id_opt = ctx.ids.to_internal(PublicChannelId{channel_id_raw.value()});
    if (!channel_id_opt.has_value()) {
        return std::unexpected(CommandError{4, "Channel not found"});
    }

    auto channel_opt = ctx.channel_service.getChannel(*channel_id_opt);
    if (!channel_opt.has_value()) {
        return std::unexpected(CommandError{5, "Channel not found"});
    }
    const Channel channel = channel_opt.value();

    auto role = ctx.hub_service.getMembershipRole(channel.hub_id, user_id);
    if (!role.has_value() || (*role != Role::OWNER && *role != Role::ADMIN)) {
        return std::unexpected(
            CommandError{6, "Only admins/owners can delete channels"});
    }

    const auto public_hub_id = ctx.ids.to_public(channel.hub_id);
    const auto public_channel_id = ctx.ids.to_public(channel.id);

    // Fan out before deleting to capture subscribers
    CommandSuccess res;
    json payload = {{"type", "channel_deleted"},
                    {"hub_id", public_hub_id.value},
                    {"channel_id", public_channel_id.value}};

    auto subs = ctx.subscription_manager.getSubscribers(Topic::HubTopic(channel.hub_id));
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
    res.intents.push_back(Unicast{.conn = input->conn, .payload = payload});

    ctx.channel_service.deleteChannel(channel.id, channel.hub_id);
    ctx.subscription_manager.unsubscribe(user_id, Topic::ChannelTopic(channel.hub_id, channel.id));
    return res;
}

}  // namespace app
