#include "app/commands/CreateHubCommand.h"

#include "domains/Channel.h"
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

CreateHubCommand::CreateHubCommand(ServiceObjects& svc_objs)
    : services_(svc_objs) {}

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
    const auto& input = ctx.input;
    auto& output = ctx.output;

    if (!ctx.snapshot.authenticated) {
        output.success = false;
        output.error_code = "not_authenticated";
        output.error_message = "Authenticate before creating hubs.";
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

    std::string name = sanitize_name(input.data.value("name", std::string{}));
    if (name.empty()) {
        output.success = false;
        output.error_code = "invalid_name";
        output.error_message = "Hub name is required.";
        json err = {
            {"type", "error"}, {"code", "invalid_name"}, {"message", "Hub name is required"}};

        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
        return;
    }

    try {
        HubId hub_id = services_.db_.hubs().createHub(name, ctx.snapshot.user_id);
        services_.ids_.to_public(hub_id);

        ChannelId general_id{""};
        try {
            general_id = services_.db_.channels().createChannel(hub_id, "general", "text");
            services_.ids_.to_public(general_id);
        } catch (const std::exception&) {
            // Hub exists even if channel creation fails; continue
        }

        const auto public_hub_id = services_.ids_.to_public(hub_id);
        json channels = json::array();
        if (!general_id.value.empty()) {
            const auto public_channel_id = services_.ids_.to_public(general_id);
            channels.push_back({{"id", public_channel_id.value},
                                {"hub_id", public_hub_id.value},
                                {"name", "general"},
                                {"type", channel_type_to_string(ChannelType::CHAT)}});
        }

        services_.hub_publisher_.publish_hub(hub_id);

        output.success = true;
        output.error_code.clear();
        output.error_message.clear();
        json data =
            json{{"type", "hub_created"},
                 {"hub", json{{"id", public_hub_id.value}, {"name", name}, {"role", "owner"}}},
                 {"channels", std::move(channels)}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = data.dump();

        auto& snapshot = ctx.snapshot;
        snapshot.hubs.insert(hub_id);
        snapshot.roles[hub_id] = Role::OWNER;

        services_.gateway_.subscribe(ctx.conn_id, services_.hub_publisher_.topic_for(hub_id));

        msg.apply_psd = [snapshot](net::PerSocketData* psd) {
            if (!psd) return;

            psd->snapshot = std::make_shared<net::Snapshot>(snapshot);
        };
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
    } catch (const std::exception& ex) {
        output.success = false;
        output.error_code = "create_hub_failed";
        output.error_message = ex.what();
        json err = {{"type", "error"}, {"code", "create_hub_failed"}, {"message", ex.what()}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        ctx.output.messages.push_back(std::move(msg));
        output.sent_at = std::chrono::system_clock::now();
    }
}

}  // namespace app
