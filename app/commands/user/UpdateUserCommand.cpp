#include "app/commands/user/UpdateUserCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "proto/command/user.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"
#include "proto/event/user.pb.h"

#include <cassert>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace app {

namespace {
constexpr size_t kMaxUsernameSize = 48;
constexpr size_t kMaxAvatarSeedSize = 64;

std::string make_user_profile_updated(const UserId& user_id, const std::string& username,
                                      const std::string& avatar_seed) {
    sercom::protocol::event::UserProfileUpdated updated;
    auto* proto_user = updated.mutable_user();
    proto_user->set_id(user_id.value);
    proto_user->mutable_metadata()->set_username(username);
    proto_user->mutable_metadata()->set_avatar_seed(avatar_seed);

    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(sercom::protocol::Envelope::USER_PROFILE_UPDATED);
    updated.SerializeToString(env.mutable_payload());
    return env.SerializeAsString();
}

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
                                              "Must be authenticated to fetch messages"));
        return out;
    }

    std::optional<std::string> username_opt;
    std::optional<std::string> avatar_seed_opt;

    if (cmd.has_username()) {
        auto username = sanitize(cmd.username());
        if (username.empty() || username.size() > kMaxUsernameSize) {
            out.emplace_back(make_command_error(
                event->conn_id, env.type(),
                sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                "Username length must be between 0 and " + std::to_string(kMaxUsernameSize)));
            return out;
        }
        username_opt = std::move(username);
    }

    if (cmd.has_avatar_seed()) {
        auto seed = sanitize(cmd.avatar_seed());
        if (seed.empty() || seed.size() > kMaxAvatarSeedSize) {
            out.emplace_back(make_command_error(
                event->conn_id, env.type(),
                sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                "Avatar seed must be between 0 and " + std::to_string(kMaxAvatarSeedSize)));
            return out;
        }
        avatar_seed_opt = std::move(seed);
    }

    const UserId user_id = user_exp.value();
    auto update_res = ctx.user_service.updateSettings(user_id, username_opt, avatar_seed_opt);
    if (!update_res) {
        out.emplace_back(make_command_error(
            event->conn_id, env.type(), sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
            "Unable to update user settings"));
    }

    std::string final_username;
    std::string final_avatar_seed;
    if (auto user = ctx.user_service.getUser(user_id)) {
        final_username = user->username;
        final_avatar_seed = user->avatar_seed;
    } else {
        out.emplace_back(make_drop_connection(
            event->conn_id, sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
            "Unable to fetch updated user information"));
    }

    std::string bytes = make_user_profile_updated(user_id, final_username, final_avatar_seed);

    std::unordered_set<GlobalConnId> conn_set;
    conn_set.insert(event->conn_id);

    const auto hubs = ctx.hub_service.getUserHubs(user_id);
    for (const auto& hub : hubs) {
        utils::metrics::counters().fanout_subscriber_snapshot_total.fetch_add(
            1, std::memory_order_relaxed);
        auto subs = ctx.subscription_manager.getSubscribers(Topic::HubTopic(hub.id));
        if (!subs) {
            continue;
        }
        for (const auto& sub : *subs) {
            conn_set.insert(sub);
        }
    }

    std::vector<GlobalConnId> conns;
    for (const auto& conn : conn_set) {
        conns.push_back(conn);
    }

    out.emplace_back(
        make_outgoing_message(net::outbound::Target::many(std::move(conns)), std::move(bytes)));
    return out;
}

}  // namespace app
