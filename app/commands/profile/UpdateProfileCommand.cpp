#include "app/commands/profile/UpdateProfileCommand.h"

#include "app/dispatcher/CommandContext.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

using nlohmann::json;

namespace app {

namespace {
constexpr std::size_t kMaxNameLength = 48;
}

std::string UpdateProfileCommand::trim(std::string value) {
    value.erase(value.begin(), std::find_if(value.begin(), value.end(),
                                            [](unsigned char ch) { return !std::isspace(ch); }));
    value.erase(std::find_if(value.rbegin(), value.rend(),
                             [](unsigned char ch) { return !std::isspace(ch); })
                    .base(),
                value.end());
    return value;
}

CommandResult UpdateProfileCommand::execute(CommandContext& ctx, const CommandInput cmd) {
    const auto* input = std::get_if<JsonInput>(&cmd);
    if (!input) {
        return std::unexpected(CommandError{1, "update_profile expects JSON input"});
    }

    auto user_exp = ctx.session_manager.sessionOfConnection(input->conn);
    if (!user_exp.has_value()) {
        return std::unexpected(CommandError{2, "Authenticate first"});
    }
    const UserId user_id = user_exp.value();

    std::optional<std::string> username_opt;
    std::optional<std::string> full_name_opt;
    const auto j = json::parse(input->body, nullptr, false);
    if (j.is_discarded()) {
        return std::unexpected(CommandError{3, "Invalid JSON"});
    }
    if (j.contains("username") && j["username"].is_string()) {
        auto value = trim(j["username"].get<std::string>());
        if (!value.empty()) {
            if (value.size() > kMaxNameLength) value.resize(kMaxNameLength);
            username_opt = value;
        } else {
            username_opt = std::string{};
        }
    }
    if (j.contains("full_name") && j["full_name"].is_string()) {
        auto value = trim(j["full_name"].get<std::string>());
        if (!value.empty()) {
            if (value.size() > kMaxNameLength) value.resize(kMaxNameLength);
            full_name_opt = value;
        } else {
            full_name_opt = std::string{};
        }
    }

    if (!username_opt.has_value() && !full_name_opt.has_value()) {
        return std::unexpected(CommandError{3, "Provide a username or full name to update"});
    }

    try {
        ctx.user_service.updateProfile(user_id, username_opt, full_name_opt);

        std::string final_username;
        std::string final_full_name;
        if (auto u = ctx.user_service.getUser(user_id)) {
            final_username = u->username;
            final_full_name = u->full_name;
        } else {
            final_username = username_opt.value_or(std::string{});
            final_full_name = full_name_opt.value_or(std::string{});
        }

        std::string chosen_display;
        if (!final_username.empty()) {
            chosen_display = final_username;
        } else if (!final_full_name.empty()) {
            chosen_display = final_full_name;
        }
        if (chosen_display.empty()) chosen_display = "Member";

        json payload = {
            {"type", "profile_updated"},      {"success", true},
            {"username", final_username},     {"full_name", final_full_name},
            {"display_name", chosen_display}, {"user_id", ctx.ids.to_public(user_id).value}};

        CommandSuccess res;
        res.intents.push_back(Unicast{.conn = input->conn, .payload = payload});

        // Broadcast updated name to hub members
        const auto hubs = ctx.hub_service.getUserHubs(user_id);
        for (const auto& hub : hubs) {
            const auto online_members = ctx.presence_manager.onlineUsersInHub(hub.id);
            std::vector<GlobalConnId> conns;
            conns.reserve(online_members.size());
            for (const auto& member_id : online_members) {
                if (member_id == user_id) continue;
                auto c = ctx.session_manager.getMainConnection(member_id);
                if (c.has_value()) conns.push_back(c.value());
            }
            if (!conns.empty()) {
                json presence = {{"type", "profile_updated"},
                                 {"hub_id", ctx.ids.to_public(hub.id).value},
                                 {"user_id", ctx.ids.to_public(user_id).value},
                                 {"username", final_username},
                                 {"full_name", final_full_name},
                                 {"display_name", chosen_display}};
                res.intents.push_back(
                    Fanout{.conns = std::move(conns), .payload = std::move(presence)});
            }
        }

        return res;
    } catch (const std::exception& ex) {
        return std::unexpected(CommandError{4, ex.what()});
    }
}

}  // namespace app
