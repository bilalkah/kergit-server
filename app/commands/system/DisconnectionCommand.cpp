#include "app/commands/system/DisconnectionCommand.h"

#include "app/managers/subscription/Topic.h"

#include <nlohmann/json.hpp>
#include <variant>
#include <vector>

using nlohmann::json;

namespace app {

std::vector<net::outbound::OutgoingMessage> DisconnectionCommand::execute(CommandContext& ctx,
                                                                          const queue::Event& evt) {
    std::vector<net::outbound::OutgoingMessage> out;
    const auto* event = std::get_if<queue::DisconnectionEvent>(&evt);
    if (!event) {
        std::cout << "DisconnectionCommand: invalid event type" << std::endl;
        return out;
    }

    auto session_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!session_exp.has_value()) {
        return out;  // unknown connection, ignore
    }

    const UserId user_id = session_exp.value();
    const auto hubid_list = [&]() {
        std::vector<HubId> hubid_list;
        const auto user_subs = ctx.subscription_manager.getSubscriptions(user_id);
        if (!user_subs.has_value()) {
            return hubid_list;
        }
        for (const auto& topic : user_subs.value()) {
            if (topic.kind != TopicKind::Hub) continue;
            hubid_list.emplace_back(topic_utils::extractHubId(topic));
        }
        return hubid_list;
    }();

    ctx.session_manager.removeConnection(event->conn_id);
    ctx.subscription_manager.removeAllForUser(user_id);

    for (const auto& hub_id : hubid_list) {
        const auto online_members = ctx.presence_manager.onlineUsersInHub(hub_id);
        std::vector<GlobalConnId> recipients;
        recipients.reserve(online_members.size());

        for (const auto& member_id : online_members) {
            if (member_id == user_id) continue;
            auto conn_exp = ctx.session_manager.getMainConnection(member_id);
            if (!conn_exp.has_value()) continue;
            recipients.push_back(conn_exp.value());
        }

            if (!recipients.empty()) {
                auto payload = ctx.hub_notifier.memberOffline(hub_id, user_id);
                out.push_back(net::outbound::OutgoingMessage{
                    .priority = net::outbound::OutboundPriority::Low,
                    .target = net::outbound::Target::many(std::move(recipients)),
                    .action = net::outbound::SendPayload{.payload = std::move(payload)}});
            }
    }

    return out;
}

}  // namespace app
