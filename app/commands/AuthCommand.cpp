#include "app/commands/AuthCommand.h"

using namespace app::services;

namespace app {

AuthCommand::AuthCommand() {}

void AuthCommand::execute(CommandContext& ctx) {
    const auto& input = ctx.input;
    auto& psd = ctx.psd;
    auto& output = ctx.output;

    const std::string token = input.data.value("token", "");

    if (token.empty()) {
        output.success = false;
        output.data = {{"type", "auth_response"}, {"success", false}, {"error", "missing_token"}};
        return;
    }

    auto auth_result = auth_service_.authenticate(token);

    if (!auth_result.success) {
        output.success = false;
        output.data["type"] = "auth_response";
        output.data["success"] = false;
        output.data["error_message"] = auth_result.error_message;
        return;
    }

    const auto& claims = auth_result.claims;

    fill_psd(psd, claims);

    output.success = true;
    output.data = {{"type", "auth_response"}, {"success", true}};
    output.sent_at = std::chrono::system_clock::now();
}

void AuthCommand::fill_psd(net::PerSocketData& psd,
                           const infra::security::token::UserClaims& claims) {
    psd.user_id = claims.id;
    psd.authenticated = true;
}

};  // namespace app
