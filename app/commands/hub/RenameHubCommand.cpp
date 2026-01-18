#include "app/commands/hub/RenameHubCommand.h"

#include "app/commands/utils.h"
#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "domains/Hub.h"
#include "proto/command/hub.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"
#include "proto/event/hub.pb.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace app {

std::string RenameHubCommand::sanitize(std::string name) {
    auto trim = [](std::string& s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                                        [](unsigned char ch) { return !std::isspace(ch); }));
        s.erase(
            std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); })
                .base(),
            s.end());
    };
    trim(name);
    if (name.size() > 64) name.resize(64);
    return name;
}

std::vector<net::outbound::OutgoingMessage> RenameHubCommand::execute(CommandContext& ctx,
                                                                      const queue::Event& evt) {
    const auto* event = std::get_if<queue::MessageEvent>(&evt);
    if (!event) {
        return {};
    }

    const auto& env = event->payload.env;
    if (env.type() != sercom::protocol::Envelope::HUB_RENAME) {
        return {};
    }

    sercom::protocol::command::RenameHub cmd;
    if (!cmd.ParseFromString(env.payload())) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INVALID_FORMAT,
                                   "Invalid HUB_RENAME payload")};
    }

    auto user_exp = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (!user_exp.has_value()) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_UNAUTHORIZED,
                                   "Authenticate first")};
    }
    const UserId user_id = user_exp.value();

    std::string requested_name = sanitize(cmd.name());
    if (requested_name.empty()) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INVALID_ARGUMENT,
                                   "Hub name is required")};
    }

    auto hub_id_opt = ctx.ids.to_internal(PublicHubId{cmd.hub_id()});
    if (!hub_id_opt.has_value()) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_NOT_FOUND,
                                   "Hub not found")};
    }
    const HubId hub_id = hub_id_opt.value();

    if (!ctx.hub_service.isHubMember(hub_id, user_id)) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                   "Join the hub before renaming it")};
    }

    auto role = ctx.hub_service.getMembershipRole(hub_id, user_id);
    if (!role.has_value() || (*role != Role::OWNER && *role != Role::ADMIN)) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_FORBIDDEN,
                                   "Only admins or owners can rename hubs")};
    }

    try {
        if (!ctx.hub_service.renameHub(hub_id, requested_name)) {
            return {make_command_error(event->conn_id, env.type(),
                                       sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
                                       "Unable to rename hub at this time")};
        }
    } catch (const std::exception& ex) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
                                   ex.what())};
    } catch (...) {
        return {make_command_error(event->conn_id, env.type(),
                                   sercom::protocol::event::CommandErrorCode_INTERNAL_ERROR,
                                   "Unable to rename hub at this time")};
    }

    sercom::protocol::event::HubRenamed renamed;
    renamed.set_hub_id(ctx.ids.to_public(hub_id).value);
    renamed.set_name(requested_name);

    sercom::protocol::Envelope out_env;
    out_env.set_version(1);
    out_env.set_type(sercom::protocol::Envelope::HUB_RENAMED);
    renamed.SerializeToString(out_env.mutable_payload());

    std::string bytes;
    out_env.SerializeToString(&bytes);

    std::vector<GlobalConnId> conns;
    auto subs = ctx.subscription_manager.getSubscribers(Topic::HubTopic(hub_id));
    if (subs.has_value()) {
        conns.reserve(subs->size());
        for (const auto& uid : subs.value()) {
            auto conn = ctx.session_manager.getMainConnection(uid);
            if (conn.has_value()) conns.push_back(conn.value());
        }
    }

    if (conns.empty()) {
        return {};
    }

    return {net::outbound::OutgoingMessage{
        .target = net::outbound::Target::many(std::move(conns)),
        .action = net::outbound::SendPayload{
            .payload = net::outbound::Payload{.data = std::move(bytes), .is_binary = true}}}};
}

}  // namespace app
