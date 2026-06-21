#include "app/commands/session/DisconnectionCommand.h"

#include "app/commands/utils.h"
#include "app/managers/subscription/Topic.h"
#include "utils/EventLogger.h"

#include <string>
#include <variant>
#include <vector>

namespace app {

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

    const auto hub_ids = [&]() {
        std::vector<HubId> list;
        const auto conn_subs =
            ctx.subscription_manager.getSubscriptionsForConnection(event->conn_id);
        if (!conn_subs.has_value()) return list;
        for (const auto& topic : conn_subs.value()) {
            if (topic.kind != TopicKind::Hub) {
                continue;
            }
            list.emplace_back(topic_utils::extractHubId(topic));
        }
        return list;
    }();

    ctx.session_manager.removeConnection(event->conn_id);
    ctx.subscription_manager.removeAllForConnection(event->conn_id);

    if (ctx.session_manager.hasSession(user_id)) {
        return out;
    }

    ctx.voice_service.on_session_destroyed(user_id);

    for (const auto& hub_id : hub_ids) {
        utils::metrics::counters().fanout_subscriber_snapshot_total.fetch_add(
            1, std::memory_order_relaxed);
        auto subs = ctx.subscription_manager.getSubscribers(Topic::HubTopic(hub_id));
        if (!subs || subs->empty()) {
            continue;
        }

        std::vector<GlobalConnId> recipients;
        recipients.reserve(subs->size());
        for (const auto& conn : *subs) {
            recipients.push_back(conn);
        }
        if (recipients.empty()) {
            continue;
        }

        sercom::protocol::event::RtSignal signal;
        auto* presence = signal.mutable_presence();
        presence->set_hub_id(hub_id.value);
        presence->set_user_id(user_id.value);
        presence->set_is_online(false);

        out.emplace_back(make_outgoing_message(net::outbound::Target::many(std::move(recipients)),
                                               make_rt_signal(signal)));
    }

    ctx.audit_service.log(AuditRepository::Event{
        .category = "auth",
        .event_type = "auth.logout",
        .severity = "info",
        .actor_type = "user",
        .actor_user_id = user_id,
        .session_id = std::to_string(session_id),
        .connection_id = to_string(event->conn_id),
    });

    return out;
}

}  // namespace app
