#include "app/commands/session/DisconnectionCommand.h"

#include "app/commands/utils.h"
#include "app/managers/subscription/Topic.h"
#include "utils/EventLogger.h"

#include <string>
#include <unordered_set>
#include <variant>
#include <vector>

namespace app {

namespace {

net::outbound::OutgoingMessage make_broadcast(std::vector<GlobalConnId> conns, std::string bytes) {
    return net::outbound::OutgoingMessage{
        .priority = net::outbound::OutboundPriority::Low,
        .target = net::outbound::Target::many(std::move(conns)),
        .action = net::outbound::Action{
            std::in_place_type<net::outbound::SendPayload>,
            net::outbound::SendPayload{.payload = net::outbound::Payload{std::move(bytes), true}}}};
}

}  // namespace

std::vector<net::outbound::OutgoingMessage> DisconnectionCommand::execute(CommandContext& ctx,
                                                                          const queue::Event& evt) {
    std::vector<net::outbound::OutgoingMessage> out;
    const auto* event = std::get_if<queue::DisconnectionEvent>(&evt);
    if (!event) {
        utils::EventLogger::instance().log(utils::EventCategory::DEBUG, "", "DISCONNECT_INVALID", 0,
                                           "DisconnectionCommand: invalid event type");
        return out;
    }

    auto session_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!session_exp.has_value()) {
        return out;
    }

    auto session_id_exp = ctx.session_manager.sessionIdOfConnection(event->conn_id);
    if (!session_id_exp.has_value()) {
        return out;
    }

    const UserId user_id = session_exp.value();
    const SessionId session_id = session_id_exp.value();

    utils::EventLogger::instance().log(
        utils::EventCategory::SESSION, user_id.value, "connection_closed", 0,
        "session_id=" + std::to_string(session_id) + " close_code=" + std::to_string(event->code) +
            " reason=" + event->reason);

    const auto hubid_list = [&]() {
        std::vector<HubId> list;
        const auto conn_subs =
            ctx.subscription_manager.getSubscriptionsForConnection(event->conn_id);
        if (!conn_subs.has_value()) return list;
        for (const auto& topic : conn_subs.value()) {
            if (topic.kind != TopicKind::Hub) continue;
            list.emplace_back(topic_utils::extractHubId(topic));
        }
        return list;
    }();

    ctx.session_manager.removeConnection(event->conn_id);
    ctx.subscription_manager.removeAllForConnection(event->conn_id);

    if (!ctx.session_manager.hasSession(user_id)) {
        for (const auto& hub_id : hubid_list) {
            const auto online_members = ctx.presence_manager.onlineUsersInHub(hub_id);
            std::unordered_set<GlobalConnId> recipient_set;

            for (const auto& member_id : online_members) {
                if (member_id == user_id) continue;
                const auto member_conns = ctx.session_manager.getSessionConnections(member_id);
                for (const auto& conn : member_conns) {
                    recipient_set.insert(conn);
                }
            }

            std::vector<GlobalConnId> recipients(recipient_set.begin(), recipient_set.end());
            if (!recipients.empty()) {
                auto payload = ctx.hub_notifier.memberOffline(hub_id, user_id);
                out.push_back(make_broadcast(std::move(recipients), std::move(payload)));
            }
        }
    }

    return out;
}

}  // namespace app
