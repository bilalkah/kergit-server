#include "app/commands/hub/CreateHubCommand.h"

#include "app/dispatcher/CommandContext.h"
#include "app/managers/subscription/Topic.h"
#include "domains/Channel.h"
#include "domains/Hub.h"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

using nlohmann::json;

namespace app {

namespace {
std::string channel_type_to_string(ChannelType type) {
    return type == ChannelType::VOICE ? "voice" : "text";
}
}  // namespace

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

CommandResult CreateHubCommand::execute(CommandContext& ctx, const CommandInput cmd) {
    const auto* input = std::get_if<JsonInput>(&cmd);
    if (!input) {
        return std::unexpected(CommandError{"invalid_input", "create_hub expects JSON input"});
    }

    auto user_exp = ctx.session_manager.sessionOfConnection(input->conn);
    if (!user_exp.has_value()) {
        return std::unexpected(CommandError{"not_authenticated", "Authenticate first"});
    }
    const UserId user_id = user_exp.value();

    std::string name = sanitize_name(input->body.value("name", std::string{}));
    if (name.empty()) {
        return std::unexpected(CommandError{"invalid_name", "Hub name is required"});
    }

    try {
        HubId hub_id = ctx.hub_service.createHub(name, user_id);
        const auto public_hub_id = ctx.ids.to_public(hub_id);

        // Create default "general" channel
        ChannelId general_id{""};
        try {
            general_id = ctx.channel_service.createChannel(hub_id, "general", "text");
        } catch (const std::exception&) {
            // proceed without default channel
        }

        json channels = json::array();
        if (!general_id.value.empty()) {
            const auto public_channel_id = ctx.ids.to_public(general_id);
            channels.push_back({{"id", public_channel_id.value},
                                {"hub_id", public_hub_id.value},
                                {"name", "general"},
                                {"type", channel_type_to_string(ChannelType::CHAT)}});
        }

        std::string owner_display = "Member";
        if (auto u = ctx.user_service.getUser(user_id)) {
            if (!u->username.empty()) owner_display = u->username;
            else if (!u->full_name.empty()) owner_display = u->full_name;
        }

        json hub_json = {{"id", public_hub_id.value}, {"name", name}, {"role", "owner"}};
        json members = json::array(
            {{ {"handle", owner_display},
               {"display_name", owner_display},
               {"online", true},
               {"user_id", ctx.ids.to_public(user_id).value} }});
        json payload = {{"type", "hub_created"},
                        {"hub", hub_json},
                        {"channels", channels},
                        {"members", std::move(members)}};

        CommandSuccess res;
        res.intents.push_back(Unicast{.conn = input->conn, .payload = payload});

        // Subscribe creator to hub topic
        ctx.subscription_manager.subscribe(user_id, Topic::HubTopic(hub_id));

        return res;
    } catch (const std::exception& ex) {
        // Surface DB/business errors to client instead of terminating the worker
        std::string raw = ex.what() ? ex.what() : "Unable to create hub";
        // Strip noisy Postgres CONTEXT
        auto ctx_pos = raw.find("CONTEXT:");
        if (ctx_pos != std::string::npos) raw = raw.substr(0, ctx_pos);
        std::string msg = raw;
        std::string code = "create_hub_failed";
        if (raw.find("ownership limit") != std::string::npos) {
            code = "hub_limit_reached";
            msg = "Hub ownership limit reached (max 2 hubs per user). Delete a hub to create another.";
        }
        return std::unexpected(CommandError{code, msg});
    }
}

}  // namespace app
