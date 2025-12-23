#include "app/commands/CreateChannelCommand.h"

#include "app/services/HubPublisher.h"
#include "app/services/PublicIdService.h"
#include "infra/persistence/PersistenceGateway.h"
#include "net/PerSocketData.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <nlohmann/json.hpp>
#include <stdexcept>

using nlohmann::json;

namespace app {

CreateChannelCommand::CreateChannelCommand(PersistenceGateway& db,
                                           app::services::HubPublisher& hub_publisher,
                                           app::services::PublicIdService& ids)
    : db_(db), hub_publisher_(hub_publisher), ids_(ids) {}

bool CreateChannelCommand::has_privilege(const net::Snapshot& snapshot, const HubId& hub_id) {
    auto it = snapshot.roles.find(hub_id);
    if (it == snapshot.roles.end()) return false;

    Role role = it->second;
    return role == Role::OWNER || role == Role::ADMIN;
}

void CreateChannelCommand::execute(CommandContext& ctx) {
    const auto& data = ctx.input.data;
    auto& output = ctx.output;

    if (!ctx.authenticated) {
        output.success = false;
        output.error_code = "not_authenticated";
        output.error_message = "Authentication required";

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

    const std::string hub_id_str = data.value("hub_id", "");
    const std::string name_raw = data.value("name", "");
    const std::string type = data.value("type", "text");

    if (hub_id_str.empty()) {
        output.success = false;
        output.error_code = "missing_hub_id";
        output.error_message = "hub_id is required";
        json err = {
            {"type", "error"}, {"code", "missing_hub_id"}, {"message", "hub_id is required"}};

        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();

        return;
    }

    auto hub_id_opt = ids_.to_internal(PublicHubId{hub_id_str});
    if (!hub_id_opt.has_value()) {
        output.success = false;
        output.error_code = "not_in_hub";
        output.error_message = "Join the hub before creating channels";

        json err = {{"type", "error"},
                    {"code", "not_in_hub"},
                    {"message", "Join the hub before creating channels"}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();

        return;
    }

    const auto& snapshot = ctx.snapshot;

    HubId hub_id = *hub_id_opt;
    if (!snapshot.hubs.count(hub_id)) {
        output.success = false;
        output.error_code = "not_in_hub";
        output.error_message = "Join the hub before creating channels";
        json err = {{"type", "error"},
                    {"code", "not_in_hub"},
                    {"message", "Join the hub before creating channels"}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();

        return;
    }

    if (!has_privilege(snapshot, hub_id)) {
        output.success = false;
        output.error_code = "insufficient_privilege";
        output.error_message = "Only owners or admins can create channels";

        json err = {{"type", "error"},
                    {"code", "insufficient_privilege"},
                    {"message", "Only owners or admins can create channels"}};

        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();

        return;
    }

    std::string name = name_raw;
    auto trim = [](std::string& s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                                        [](unsigned char ch) { return !std::isspace(ch); }));
        s.erase(
            std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); })
                .base(),
            s.end());
    };
    trim(name);

    if (name.empty()) {
        output.success = false;
        output.error_code = "invalid_name";
        output.error_message = "Channel name is required";
        json err = {
            {"type", "error"}, {"code", "invalid_name"}, {"message", "Channel name is required"}};

        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
        return;
    }

    if (name.size() > 48) name.resize(48);

    std::string channel_type = type == "voice" ? "voice" : "text";

    try {
        ChannelId created = db_.channels().createChannel(hub_id, name, channel_type);
        const auto public_hub_id = ids_.to_public(hub_id);
        const auto public_channel_id = ids_.to_public(created);

        nlohmann::json channel_json = {{"id", public_channel_id.value},
                                       {"hub_id", public_hub_id.value},
                                       {"name", name},
                                       {"type", channel_type}};

        output.success = true;
        output.error_code.clear();
        output.error_message.clear();
        json data = {{"type", "channel_created"},
                    {"hub_id", public_hub_id.value},
                    {"channel", channel_json}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = data.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();

        hub_publisher_.publish_hub(hub_id);
    } catch (const std::exception& ex) {
        output.success = false;
        output.error_code = "create_failed";
        output.error_message = ex.what();
        json err = {{"type", "error"}, {"code", "create_failed"}, {"message", ex.what()}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
    }
}

}  // namespace app
