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
    const auto& input = ctx.input;
    auto& output = ctx.output;

    if (!ctx.authenticated) {
        output.success = false;
        output.error_code = "not_authenticated";
        output.error_message = "Authenticate before updating profile.";
        json err = {{"type", "error"},
                    {"code", "not_authenticated"},
                    {"message", "Authentication required"}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        output.messages.push_back(msg);
        output.sent_at = std::chrono::system_clock::now();
        return;
    }

    std::optional<std::string> username_opt;
    std::optional<std::string> full_name_opt;

    if (input.data.contains("username") && input.data["username"].is_string()) {
        auto value = trim(input.data["username"].get<std::string>());
        if (!value.empty()) {
            if (value.size() > kMaxNameLength) value.resize(kMaxNameLength);
            username_opt = value;
        } else {
            username_opt = std::string{};
        }
    }
    if (input.data.contains("full_name") && input.data["full_name"].is_string()) {
        auto value = trim(input.data["full_name"].get<std::string>());
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
        json err = {{"type", "error"},
                    {"code", "invalid_profile"},
                    {"message", "Provide a username or full name to update"}};

        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        output.messages.push_back(msg);
        output.sent_at = std::chrono::system_clock::now();
        return;
    }

    try {
        auto [final_username, final_full_name] =
            db_.users().updateUserProfile(ctx.user_id, username_opt, full_name_opt);

        ctx.username = final_username;

        std::string chosen_display;
        if (!final_username.empty()) {
            chosen_display = final_username;
        } else if (!final_full_name.empty()) {
            chosen_display = final_full_name;
        }

        if (chosen_display.empty()) {
            if (auto db_name = db_.users().getUserDisplayName(ctx.user_id)) {
                if (!db_name->empty()) chosen_display = *db_name;
            }
        }
        if (chosen_display.empty()) chosen_display = "Member";

        ids_.remember_display(ctx.user_id, chosen_display);

        hub_publisher_.publish_hubs(ctx.snapshot.hubs);

        output.success = true;
        output.error_code.clear();
        output.error_message.clear();
        json data = json{{"type", "profile_updated"},
                         {"success", true},
                         {"username", final_username},
                         {"full_name", final_full_name},
                         {"display_name", chosen_display}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = data.dump();
        msg.apply_psd = [final_username, final_full_name](net::PerSocketData* psd) {
            if (psd) {
                psd->username = final_username;
            }
        };
        output.messages.push_back(msg);
        output.sent_at = std::chrono::system_clock::now();
    } catch (const std::exception& ex) {
        output.success = false;
        output.error_code = "update_failed";
        output.error_message = ex.what();
        json err = {{"type", "error"}, {"code", "update_failed"}, {"message", ex.what()}};
        DirectMessage msg;
        msg.conn_id = ctx.conn_id;
        msg.payload = err.dump();
        output.messages.push_back(msg);
        output.sent_at = std::chrono::system_clock::now();
    }
}

}  // namespace app
