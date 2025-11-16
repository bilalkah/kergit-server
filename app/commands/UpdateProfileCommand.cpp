#include "app/commands/UpdateProfileCommand.h"

#include "app/services/HubPublisher.h"
#include "app/services/PublicIdService.h"
#include "infra/persistence/PersistenceGateway.h"
#include "net/PerSocketData.h"

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
}  // namespace

UpdateProfileCommand::UpdateProfileCommand(PersistenceGateway& db,
                                           app::services::HubPublisher& hub_publisher,
                                           app::services::PublicIdService& ids)
    : db_(db), hub_publisher_(hub_publisher), ids_(ids) {}

std::string UpdateProfileCommand::trim(std::string value) {
    value.erase(value.begin(), std::find_if(value.begin(), value.end(),
                                            [](unsigned char ch) { return !std::isspace(ch); }));
    value.erase(std::find_if(value.rbegin(), value.rend(),
                             [](unsigned char ch) { return !std::isspace(ch); })
                    .base(),
                value.end());
    return value;
}

void UpdateProfileCommand::execute(CommandContext& ctx) {
    auto& psd = ctx.psd;
    auto& output = ctx.output;
    const auto& data = ctx.input.data;

    if (!psd.authenticated) {
        output.success = false;
        output.error_code = "not_authenticated";
        output.error_message = "Authenticate before updating profile.";
        output.data = {{"type", "error"},
                       {"code", "not_authenticated"},
                       {"message", "Authentication required"}};
        return;
    }

    std::optional<std::string> username_opt;
    std::optional<std::string> full_name_opt;

    if (data.contains("username") && data["username"].is_string()) {
        auto value = trim(data["username"].get<std::string>());
        if (!value.empty()) {
            if (value.size() > kMaxNameLength) value.resize(kMaxNameLength);
            username_opt = value;
        } else {
            username_opt = std::string{};
        }
    }
    if (data.contains("full_name") && data["full_name"].is_string()) {
        auto value = trim(data["full_name"].get<std::string>());
        if (!value.empty()) {
            if (value.size() > kMaxNameLength) value.resize(kMaxNameLength);
            full_name_opt = value;
        } else {
            full_name_opt = std::string{};
        }
    }

    if (!username_opt.has_value() && !full_name_opt.has_value()) {
        output.success = false;
        output.error_code = "invalid_profile";
        output.error_message = "Provide a username or full name to update.";
        output.data = {{"type", "error"},
                       {"code", "invalid_profile"},
                       {"message", "Provide a username or full name to update"}};
        return;
    }

    try {
        auto [final_username, final_full_name] =
            db_.users().updateUserProfile(psd.user_id, username_opt, full_name_opt);

        psd.username = final_username;

        std::string chosen_display;
        if (!final_username.empty()) {
            chosen_display = final_username;
        } else if (!final_full_name.empty()) {
            chosen_display = final_full_name;
        } else if (!psd.username.empty()) {
            chosen_display = psd.username;
        }

        if (chosen_display.empty()) {
            if (auto db_name = db_.users().getUserDisplayName(psd.user_id)) {
                if (!db_name->empty()) chosen_display = *db_name;
            }
        }
        if (chosen_display.empty()) chosen_display = "Member";

        ids_.remember_display(psd.user_id, chosen_display);

        hub_publisher_.publish_hubs(psd.hub_memberships);

        output.success = true;
        output.error_code.clear();
        output.error_message.clear();
        output.data = json{{"type", "profile_updated"},
                           {"success", true},
                           {"username", final_username},
                           {"full_name", final_full_name},
                           {"display_name", chosen_display}};
        output.sent_at = std::chrono::system_clock::now();
    } catch (const std::exception& ex) {
        output.success = false;
        output.error_code = "update_failed";
        output.error_message = ex.what();
        output.data = {{"type", "error"}, {"code", "update_failed"}, {"message", ex.what()}};
    }
}

}  // namespace app
