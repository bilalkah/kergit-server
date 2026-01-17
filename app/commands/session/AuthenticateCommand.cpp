#include "app/commands/session/AuthenticateCommand.h"

#include "proto/command/session.pb.h"

#include <chrono>

namespace app {
CommandResult AuthenticateCommand::execute(CommandContext& ctx, const CommandInput cmd) {
    const auto* input = std::get_if<MessageEvent>(&cmd);
    if (!input) {
        return std::unexpected(
            CommandError{1, "Authenticate command expects a message event"});
    }

    sercom::protocol::command::Authenticate auth;
    if (!auth.ParseFromString(input->body)) {
        return std::unexpected(CommandError{2, "Invalid AUTH payload"});
    }

    if (auth.type() != sercom::protocol::command::AuthType_REAUTH) {
        return std::unexpected(CommandError{3, "Unsupported auth type"});
    }

    if (auth.provider() != sercom::protocol::command::AuthProvider_SUPABASE) {
        return std::unexpected(CommandError{4, "Unsupported auth provider"});
    }

    if (auth.token().empty()) {
        return std::unexpected(CommandError{5, "Token is required"});
    }

    auto auth_result = ctx.auth_service.authenticate(auth.token());
    if (!auth_result.has_value()) {
        switch (auth_result.error()) {
            case services::AuthError::InvalidToken:
                return std::unexpected(CommandError{6, "Invalid token"});
            case services::AuthError::ExpiredToken:
                return std::unexpected(CommandError{7, "Token expired"});
            default:
                return std::unexpected(CommandError{8, "Auth failed"});
        }
    }

    const auto& claims = auth_result.value();
    if (claims.id.empty()) {
        return std::unexpected(CommandError{9, "Auth claims missing user id"});
    }

    auto session_user = ctx.session_manager.sessionOfConnection(input->conn);
    if (session_user.has_value() && session_user.value().value != claims.id) {
        return std::unexpected(CommandError{10, "Auth user mismatch"});
    }

    const auto expires_at =
        std::chrono::system_clock::time_point{std::chrono::seconds{claims.exp}};

    CommandSuccess res;
    res.intents.push_back(AuthStateIntent{
        .conn = input->conn,
        .expires_at = expires_at,
        .authenticated = true,
    });
    return res;
}
}  // namespace app
