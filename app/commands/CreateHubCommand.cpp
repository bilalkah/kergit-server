#include "app/commands/CreateHubCommand.h"

#include "app/services/HubPublisher.h"
#include "app/services/PublicIdService.h"
#include "domains/Channel.h"
#include "infra/persistence/PersistenceGateway.h"
#include "net/ClientGateway.h"
#include "net/PerSocketData.h"

#include <algorithm>
#include <cctype>
#include <nlohmann/json.hpp>
#include <string>

using nlohmann::json;

namespace app {

namespace {
std::string channel_type_to_string(ChannelType type) {
    return type == ChannelType::VOICE ? "voice" : "text";
}
}  // namespace

CreateHubCommand::CreateHubCommand(PersistenceGateway& db, net::ClientGateway& gateway,
                                   app::services::HubPublisher& hub_publisher,
                                   app::services::PublicIdService& ids)
    : db_(db), gateway_(gateway), hub_publisher_(hub_publisher), ids_(ids) {}

std::string CreateHubCommand::sanitize_name(std::string name) {
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

void CreateHubCommand::execute(CommandContext& ctx) {
    auto& psd = ctx.psd;
    auto& output = ctx.output;
    const auto& data = ctx.input.data;

    if (!psd.authenticated) {
        output.success = false;
        output.error_code = "not_authenticated";
        output.error_message = "Authenticate before creating hubs.";
        output.data = {{"type", "error"},
                       {"code", "not_authenticated"},
                       {"message", "Authentication required"}};
        return;
    }

    std::string name = sanitize_name(data.value("name", std::string{}));
    if (name.empty()) {
        output.success = false;
        output.error_code = "invalid_name";
        output.error_message = "Hub name is required.";
        output.data = {
            {"type", "error"}, {"code", "invalid_name"}, {"message", "Hub name is required"}};
        return;
    }

    try {
        HubId hub_id = db_.hubs().createHub(name, psd.user_id);
        ids_.to_public(hub_id);

        ChannelId general_id{""};
        try {
            general_id = db_.channels().createChannel(hub_id, "general", "text");
            ids_.to_public(general_id);
        } catch (const std::exception&) {
            // Hub exists even if channel creation fails; continue
        }

        psd.hub_memberships.insert(hub_id);
        psd.hub_roles[hub_id] = Role::OWNER;

        gateway_.subscribe(psd.conn_id, app::services::HubPublisher::topic_for(hub_id));

        const auto public_hub_id = ids_.to_public(hub_id);
        json channels = json::array();
        if (!general_id.value.empty()) {
            const auto public_channel_id = ids_.to_public(general_id);
            channels.push_back({{"id", public_channel_id.value},
                                {"hub_id", public_hub_id.value},
                                {"name", "general"},
                                {"type", channel_type_to_string(ChannelType::CHAT)}});
        }

        hub_publisher_.publish_hub(hub_id);

        output.success = true;
        output.error_code.clear();
        output.error_message.clear();
        output.data =
            json{{"type", "hub_created"},
                 {"hub", json{{"id", public_hub_id.value}, {"name", name}, {"role", "owner"}}},
                 {"channels", std::move(channels)}};
        output.sent_at = std::chrono::system_clock::now();
    } catch (const std::exception& ex) {
        output.success = false;
        output.error_code = "create_hub_failed";
        output.error_message = ex.what();
        output.data = {{"type", "error"}, {"code", "create_hub_failed"}, {"message", ex.what()}};
    }
}

}  // namespace app
