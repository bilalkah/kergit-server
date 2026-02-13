#include "app/commands/user/UpdateUserCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "app/proto_builders/EnvelopeBuilders.h"
#include "proto/command/user.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"
#include "proto/event/user.pb.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace app {

std::string UpdateUserCommand::sanitize(std::string value) {
    auto trim = [](std::string& s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                                        [](unsigned char ch) { return !std::isspace(ch); }));
        s.erase(
            std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); })
                .base(),
            s.end());
    };
    trim(value);
    return value;
}

std::vector<net::outbound::OutgoingMessage> UpdateUserCommand::execute(CommandContext& ctx,
                                                                       const queue::Event& evt) {
    const auto* event = std::get_if<queue::MessageEvent>(&evt);
    if (!event) {
        return {};
    }

    const auto& env = event->payload.env;
    if (env.type() != sercom::protocol::Envelope::USER_UPDATE) {
        return {};
    }

    const auto& cmd = require_parsed<sercom::protocol::command::UpdateUser>(*event);

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!user_exp.has_value()) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
            "Authenticate first"));
    }
    const UserId user_id = user_exp.value();

    std::optional<std::string> username_opt;
    std::optional<std::string> avatar_seed_opt;

    for (int i = 0; i < cmd.changes_size(); ++i) {
        const auto& change = cmd.changes(i);
        switch (change.change_case()) {
            case sercom::protocol::command::UserChange::kUsername: {
                auto username = sanitize(change.username());
                if (username.size() > 48) username.resize(48);
                if (username.empty()) {
                    return single_outgoing(make_command_error(
                        event->conn_id, env.type(),
                        sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                        "Username is required"));
                }
                username_opt = std::move(username);
                break;
            }
            case sercom::protocol::command::UserChange::kAvatarSeed: {
                auto seed = sanitize(change.avatar_seed());
                if (seed.size() > 64) seed.resize(64);
                if (seed.empty()) {
                    return single_outgoing(make_command_error(
                        event->conn_id, env.type(),
                        sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                        "Avatar seed is required"));
                }
                avatar_seed_opt = std::move(seed);
                break;
            }
            case sercom::protocol::command::UserChange::CHANGE_NOT_SET:
            default:
                return single_outgoing(
                    make_command_error(event->conn_id, env.type(),
                                       sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                                       "Invalid change type"));
        }
    }

    if (!username_opt && !avatar_seed_opt) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "No changes requested"));
    }

    auto update_res = ctx.user_service.updateSettings(user_id, username_opt, avatar_seed_opt);
    if (!update_res) {
        return single_outgoing(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
            "Unable to update user settings"));
    }

    std::string final_username;
    std::string final_avatar_seed;
    if (auto user = ctx.user_service.getUser(user_id)) {
        final_username = user->username;
        final_avatar_seed = user->avatar_seed;
    } else {
        final_username = username_opt.value_or(std::string{});
        final_avatar_seed = avatar_seed_opt.value_or(std::string{});
    }

    sercom::protocol::event::UserProfileUpdated updated;
    updated.set_user_id(ctx.ids.to_public(user_id).value);
    updated.set_username(final_username);
    updated.set_avatar_seed(final_avatar_seed);

    std::string bytes = proto_builders::serialize_envelope(
        sercom::protocol::Envelope::USER_PROFILE_UPDATED, updated);

    std::unordered_set<GlobalConnId> conn_set;
    conn_set.insert(event->conn_id);

    const auto hubs = ctx.hub_service.getUserHubs(user_id);
    for (const auto& hub : hubs) {
        utils::metrics::counters().fanout_subscriber_snapshot_total.fetch_add(
            1, std::memory_order_relaxed);
        auto subs = ctx.subscription_manager.getSubscribers(Topic::HubTopic(hub.id));
        if (!subs) continue;
        for (const auto& conn : *subs) {
            conn_set.insert(conn);
        }
    }

    if (conn_set.empty()) {
        return {};
    }

    std::vector<GlobalConnId> conns;
    conns.reserve(conn_set.size());
    for (const auto& conn : conn_set) {
        conns.push_back(conn);
    }

    return single_outgoing(net::outbound::OutgoingMessage{
        .target = net::outbound::Target::many(std::move(conns)),
        .action =
            net::outbound::Action{std::in_place_type<net::outbound::SendPayload>,
                                  net::outbound::SendPayload{
                                      .payload = net::outbound::Payload{std::move(bytes), true}}}});
}

}  // namespace app
