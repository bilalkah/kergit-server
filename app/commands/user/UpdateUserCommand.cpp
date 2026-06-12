#include "app/commands/user/UpdateUserCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "proto/command/user.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"
#include "proto/event/state.pb.h"

#include <cassert>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace app {

namespace {
constexpr size_t kMaxUsernameSize = 48;
constexpr size_t kMaxAvatarSeedSize = 64;
constexpr size_t kMaxDisplayNameSize = 48;

}  // namespace

std::vector<net::outbound::OutgoingMessage> UpdateUserCommand::execute(CommandContext& ctx,
                                                                       const queue::Event& evt) {
    assert(std::holds_alternative<queue::MessageEvent>(evt));
    const auto* event = &std::get<queue::MessageEvent>(evt);
    const auto& env = event->payload.env;
    assert(env.type() == sercom::protocol::Envelope::USER_UPDATE);

    std::vector<net::outbound::OutgoingMessage> out;
    const auto& cmd = require_parsed<sercom::protocol::command::UpdateUser>(*event);

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!user_exp.has_value()) {
        out.emplace_back(make_drop_connection(event->conn_id,
                                              sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                              "Must be authenticated to update user"));
        return out;
    }

    std::optional<std::string> username_opt;
    std::optional<std::string> display_name_opt;
    std::optional<std::string> avatar_seed_opt;

    if (cmd.has_username()) {
        auto username = sanitize(cmd.username());
        if (username.empty() || username.size() > kMaxUsernameSize) {
            out.emplace_back(make_command_error(
                event->conn_id, env.type(),
                sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                "Username length must be between 1 and " + std::to_string(kMaxUsernameSize)));
            return out;
        }
        username_opt = std::move(username);
    }

    if (cmd.has_display_name()) {
        auto display_name = sanitize(cmd.display_name());
        if (display_name.empty() || display_name.size() > kMaxDisplayNameSize) {
            out.emplace_back(
                make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                                   "Display name length must be between 1 and " +
                                       std::to_string(kMaxDisplayNameSize)));
            return out;
        }
        display_name_opt = std::move(display_name);
    }

    if (cmd.has_avatar_seed()) {
        auto seed = sanitize(cmd.avatar_seed());
        if (seed.empty() || seed.size() > kMaxAvatarSeedSize) {
            out.emplace_back(make_command_error(
                event->conn_id, env.type(),
                sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                "Avatar seed must be between 1 and " + std::to_string(kMaxAvatarSeedSize)));
            return out;
        }
        avatar_seed_opt = std::move(seed);
    }

    if (!username_opt.has_value() && !display_name_opt.has_value() &&
        !avatar_seed_opt.has_value()) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
            "No changes requested"));
        return out;
    }

    const UserId user_id = user_exp.value();
    auto update_res =
        ctx.user_service.updateProfile(user_id, username_opt, display_name_opt, avatar_seed_opt);
    if (!update_res) {
        if (update_res.error() == services::UserService::UpdateError::DuplicateUsername) {
            out.emplace_back(make_command_error(
                event->conn_id, env.type(),
                sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                "Bu kullanıcı adı kullanımda. Lütfen başka bir kullanıcı adı seçin."));
            return out;
        }
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
            "Unable to update user settings"));
        return out;
    }

    const User& final_user = *update_res;

    const auto hubs = ctx.hub_service.getUserHubs(user_id);
    std::unordered_set<GlobalConnId> self_connections;
    for (const auto& conn : ctx.session_manager.getSessionConnections(user_id)) {
        self_connections.insert(conn);
    }

    for (const auto& hub : hubs) {
        utils::metrics::counters().fanout_subscriber_snapshot_total.fetch_add(
            1, std::memory_order_relaxed);

        auto subs = ctx.subscription_manager.getSubscribers(Topic::HubTopic(hub.id));
        std::unordered_set<GlobalConnId> recipients = self_connections;
        if (subs) {
            for (const auto& conn : *subs) {
                recipients.insert(conn);
            }
        }
        if (recipients.empty()) {
            continue;
        }

        sercom::protocol::event::StateDelta delta;
        auto* hub_delta = delta.add_hubs();
        hub_delta->set_hub_id(hub.id.value);
        auto* user_upsert =
            hub_delta->add_user_ops()->mutable_upsert()->mutable_state()->mutable_user();
        *user_upsert = to_proto_user(final_user);

        std::vector<GlobalConnId> conns;
        conns.reserve(recipients.size());
        for (const auto& conn : recipients) {
            conns.push_back(conn);
        }

        out.emplace_back(make_outgoing_message(net::outbound::Target::many(std::move(conns)),
                                               make_state_delta(delta)));
    }

    return out;
}

}  // namespace app
