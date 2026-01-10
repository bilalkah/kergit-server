#include "app/commands/bootstrap/BootstrapCommand.h"

#include "app/converters/ProtoConverters.h"
#include "app/managers/subscription/Topic.h"
#include "domains/Channel.h"
#include "domains/Hub.h"
#include "proto/envelope.pb.h"
#include "proto/event/bootstrap.pb.h"

#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using nlohmann::json;

namespace app {

CommandResult BootstrapCommand::execute(CommandContext& ctx, const CommandInput cmd) {
    const auto* input = std::get_if<ConnectEvent>(&cmd);
    if (!input) {
        return std::unexpected(
            CommandError{"invalid_input", "Bootstrap command expects a connect event"});
    }

    const UserId user_id = input->user_id;
    if (user_id.value.empty()) {
        return std::unexpected(CommandError{"invalid_input", "User id is required"});
    }

    auto db_user = ctx.user_service.getUser(user_id);
    if (!db_user) {
        return std::unexpected(CommandError{"user_not_found", "User not found in database"});
    }

    const std::string display_name =
        !db_user->username.empty()
            ? db_user->username
            : (!db_user->full_name.empty() ? db_user->full_name : "Member");

    // Track session + subscribe to hubs for presence
    ctx.session_manager.createSession(input->conn, user_id);
    const auto hubs = ctx.hub_service.getUserHubs(user_id);
    for (const auto& hub : hubs) {
        ctx.subscription_manager.subscribe(user_id, Topic::HubTopic(hub.id));
    }

    sercom::protocol::event::SessionBootstrap bootstrap;
    const auto public_user_id = ctx.ids.to_public(user_id);
    auto* self = bootstrap.mutable_self();
    self->set_id(public_user_id.value);
    self->set_username(display_name);

    std::unordered_map<UserId, std::string> user_display;
    user_display.emplace(user_id, display_name);

    std::vector<Fanout> notifications;

    for (const auto& hub : hubs) {
        auto* hub_state = bootstrap.add_hubs();
        const auto public_hub_id = ctx.ids.to_public(hub.id);
        auto* hub_msg = hub_state->mutable_hub();
        hub_msg->set_id(public_hub_id.value);
        hub_msg->set_name(hub.name);

        const auto hub_channels = ctx.channel_service.getHubChannels(hub.id);
        for (const auto& channel : hub_channels) {
            auto* channel_msg = hub_state->add_channels();
            const auto public_channel = ctx.ids.to_public(channel.id);
            channel_msg->set_id(public_channel.value);
            channel_msg->set_name(channel.name);
            channel_msg->set_type(converters::to_proto_channel_type(channel.type));
        }

        std::unordered_map<UserId, Role, UserIdHash, UserIdEq> member_roles;
        if (auto hub_details = ctx.hub_service.getHub(hub.id)) {
            member_roles = hub_details->members;
        }

        const auto online_members = ctx.presence_manager.onlineUsersInHub(hub.id);
        std::unordered_set<UserId> online_set(online_members.begin(), online_members.end());

        const auto members = ctx.hub_service.getHubMembers(hub.id);
        for (const auto& [member_id, stored_display] : members) {
            std::string name = stored_display;
            if (name.empty()) {
                if (auto member_user = ctx.user_service.getUser(member_id)) {
                    if (!member_user->username.empty()) name = member_user->username;
                    else if (!member_user->full_name.empty()) name = member_user->full_name;
                }
            }
            if (name.empty() && member_id == user_id) name = display_name;
            if (name.empty()) name = "Member";

            user_display.insert_or_assign(member_id, name);

            const auto public_member = ctx.ids.to_public(member_id);

            auto* member_msg = hub_state->add_members();
            member_msg->set_hub_id(public_hub_id.value);
            member_msg->set_user_id(public_member.value);

            Role role = Role::USER;
            auto role_it = member_roles.find(member_id);
            if (role_it != member_roles.end()) {
                role = role_it->second;
            }
            member_msg->set_role(converters::to_proto_hub_role(role));

            auto* presence_msg = hub_state->add_presences();
            presence_msg->set_hub_id(public_hub_id.value);
            presence_msg->set_user_id(public_member.value);
            presence_msg->set_is_online(online_set.find(member_id) != online_set.end());
        }

        if (!online_members.empty()) {
            std::vector<GlobalConnId> recipients;
            recipients.reserve(online_members.size());

            for (const auto& member_id : online_members) {
                if (member_id == user_id) continue;
                auto conn_exp = ctx.session_manager.getMainConnection(member_id);
                if (!conn_exp.has_value()) continue;
                recipients.push_back(conn_exp.value());
            }

            if (!recipients.empty()) {
                auto payload = ctx.hub_notifier.memberOnline(hub.id, user_id);
                if (!display_name.empty()) payload["display_name"] = display_name;
                payload["username"] = display_name;
                notifications.push_back(
                    Fanout{.conns = std::move(recipients), .payload = std::move(payload)});
            }
        }
    }

    for (const auto& [member_id, name] : user_display) {
        auto* user_msg = bootstrap.add_users();
        const auto public_member = ctx.ids.to_public(member_id);
        user_msg->set_id(public_member.value);
        user_msg->set_username(name);
    }

    std::string payload;
    if (!bootstrap.SerializeToString(&payload)) {
        return std::unexpected(
            CommandError{"serialization_error", "Failed to serialize bootstrap payload"});
    }

    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(sercom::protocol::Envelope::SESSION_BOOTSTRAP);
    env.set_payload(std::move(payload));

    std::string out_bytes;
    if (!env.SerializeToString(&out_bytes)) {
        return std::unexpected(
            CommandError{"serialization_error", "Failed to serialize bootstrap envelope"});
    }

    CommandSuccess res;
    res.intents.push_back(
        BinaryUnicast{.conn = input->conn, .payload = std::move(out_bytes)});
    for (auto& notification : notifications) {
        res.intents.push_back(std::move(notification));
    }

    return res;
}

}  // namespace app
