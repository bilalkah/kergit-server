#include "app/commands/RenameHubCommand.h"

#include "net/PerSocketData.h"

#include <algorithm>
#include <cctype>
#include <nlohmann/json.hpp>
#include <string>

using nlohmann::json;

namespace app {

RenameHubCommand::RenameHubCommand(ServiceObjects& svc_objs)
    : services_(svc_objs) {}

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

bool RenameHubCommand::is_owner(const CommandContext& ctx, const HubId& hub_id) {
    if (!ctx.snapshot.hubs.count(hub_id)) {
        // check db if not in snapshot
        auto role = services_.db_.hubs().getMembershipRole(hub_id, ctx.snapshot.user_id);
        if (!role.has_value()) {
            return false;
        }
    }
    auto role = ctx.snapshot.roles.find(hub_id);
    if (role == ctx.snapshot.roles.end()) {
        return false;
    }
    return role->second == Role::OWNER;
}

void RenameHubCommand::execute(CommandContext& ctx) {
    const auto& input = ctx.input;
    auto& output = ctx.output;

    if (!ctx.snapshot.authenticated) {
        output.success = false;
        output.error_code = "not_authenticated";
        output.error_message = "Authentication required.";
        json err = {{"type", "error"},
                    {"code", "not_authenticated"},
                    {"message", "Authentication required"}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
        return;
    }

    const std::string hub_id_str = input.data.value("hub_id", "");
    std::string requested_name = input.data.value("name", std::string{});

    if (hub_id_str.empty()) {
        output.success = false;
        output.error_code = "missing_hub_id";
        output.error_message = "hub_id is required.";
        json err = {
            {"type", "error"}, {"code", "missing_hub_id"}, {"message", "hub_id is required"}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
        return;
    }

    requested_name = sanitize(std::move(requested_name));
    if (requested_name.empty()) {
        output.success = false;
        output.error_code = "invalid_hub_name";
        output.error_message = "Hub name is required.";
        json err = {
            {"type", "error"}, {"code", "invalid_hub_name"}, {"message", "Hub name is required"}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
        return;
    }

    auto internal_hub = services_.ids_.to_internal(PublicHubId{hub_id_str});
    if (!internal_hub.has_value()) {
        output.success = false;
        output.error_code = "hub_not_found";
        output.error_message = "Hub not found.";
        json err = {
            {"type", "error"}, {"code", "hub_not_found"}, {"message", "Hub does not exist"}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
        return;
    }

    if (!ctx.snapshot.hubs.count(*internal_hub)) {
        if (!services_.db_.hubs().isHubMember(*internal_hub, ctx.snapshot.user_id)) {
            output.success = false;
            output.error_code = "not_in_hub";
            output.error_message = "Join the hub before renaming it.";
            json err = {{"type", "error"},
                        {"code", "not_in_hub"},
                        {"message", "Join the hub before renaming it"}};
            DirectMessage msg;
            msg.conn_id = ctx.conn_id;
            msg.payload = err.dump();
            ctx.output.messages.push_back(std::move(msg));
            output.sent_at = std::chrono::system_clock::now();
            return;
        }
        // psd.hub_memberships.insert(*internal_hub);
    }

    if (!is_owner(ctx, *internal_hub)) {
        output.success = false;
        output.error_code = "insufficient_privilege";
        output.error_message = "Only owners can rename hubs.";
        json err = {{"type", "error"},
                    {"code", "insufficient_privilege"},
                    {"message", "Only owners can rename hubs"}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
        return;
    }

    try {
        if (!services_.db_.hubs().renameHub(*internal_hub, requested_name)) {
            output.success = false;
            output.error_code = "rename_hub_failed";
            output.error_message = "Unable to rename hub.";
            json err = {{"type", "error"},
                        {"code", "rename_hub_failed"},
                        {"message", "Unable to rename hub"}};
            DirectMessage msg;
            msg.conn_id = ctx.conn_id;
            msg.payload = err.dump();
            ctx.output.messages.push_back(std::move(msg));
            output.sent_at = std::chrono::system_clock::now();
            return;
        }

        const auto public_hub_id = services_.ids_.to_public(*internal_hub);
        services_.hub_publisher_.publish_hub(*internal_hub);

        json event = {
            {"type", "hub_renamed"}, {"hub_id", public_hub_id.value}, {"name", requested_name}};

        PublishMessage pub_msg;
        pub_msg.topic = app::services::HubPublisher::topic_for(*internal_hub);
        pub_msg.payload = event.dump();
        ctx.output.messages.push_back(std::move(pub_msg));
        output.success = true;
        output.error_code.clear();
        output.error_message.clear();
        output.sent_at = std::chrono::system_clock::now();
    } catch (const std::exception& ex) {
        output.success = false;
        output.error_code = "rename_hub_failed";
        output.error_message = ex.what();
        json err = {{"type", "error"}, {"code", "rename_hub_failed"}, {"message", ex.what()}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
    }
}

}  // namespace app
