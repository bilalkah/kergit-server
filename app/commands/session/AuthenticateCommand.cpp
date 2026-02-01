#include "app/commands/session/AuthenticateCommand.h"

#include "app/commands/utils.h"
#include "proto/command/session.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"

#include <chrono>

namespace app {
std::vector<net::outbound::OutgoingMessage> AuthenticateCommand::execute(CommandContext& ctx,
                                                                         const queue::Event& evt) {
    std::vector<net::outbound::OutgoingMessage> result;
    const auto* event = std::get_if<queue::MessageEvent>(&evt);
    if (!event) {
        std::cout << "AuthenticateCommand: invalid event type" << std::endl;
        return {};
    }

    const auto& env = event->payload.env;
    if (env.type() != sercom::protocol::Envelope::AUTH) {
        std::cout << "AuthenticateCommand: unexpected envelope type" << std::endl;
        return {};
    }

    const auto& auth = require_parsed<sercom::protocol::command::Authenticate>(*event);

    // 4. Validate command fields
    if (auth.type() != sercom::protocol::command::AuthType_REAUTH &&
        auth.type() != sercom::protocol::command::AuthType_AUTH) {
        result.emplace_back(net::outbound::OutgoingMessage{
            .target = net::outbound::Target::one(event->conn_id),
            .action =
                net::outbound::Action{std::in_place_type<net::outbound::DropConnection>,
                                      static_cast<int>(sercom::protocol::event::CommandErrorCode::
                                                           CommandErrorCode_INVALID_ARGUMENT),
                                      "Unsupported auth type"}});
        return result;
    }

    if (auth.provider() != sercom::protocol::command::AuthProvider_SUPABASE) {
        result.emplace_back(net::outbound::OutgoingMessage{
            .target = net::outbound::Target::one(event->conn_id),
            .action =
                net::outbound::Action{std::in_place_type<net::outbound::DropConnection>,
                                      static_cast<int>(sercom::protocol::event::CommandErrorCode::
                                                           CommandErrorCode_INVALID_ARGUMENT),
                                      "Unsupported auth provider"}});
        return result;
    }

    if (auth.token().empty()) {
        result.emplace_back(net::outbound::OutgoingMessage{
            .target = net::outbound::Target::one(event->conn_id),
            .action =
                net::outbound::Action{std::in_place_type<net::outbound::DropConnection>,
                                      static_cast<int>(sercom::protocol::event::CommandErrorCode::
                                                           CommandErrorCode_INVALID_ARGUMENT),
                                      "Token is required"}});
        return result;
    }

    auto auth_result = ctx.auth_service.authenticate(auth.token());
    if (!auth_result.has_value()) {
        result.emplace_back(net::outbound::OutgoingMessage{
            .target = net::outbound::Target::one(event->conn_id),
            .action =
                net::outbound::Action{std::in_place_type<net::outbound::DropConnection>,
                                      static_cast<int>(
                                          sercom::protocol::event::CommandErrorCode_UNAUTHORIZED),
                                      "Authentication failed"}});
        return result;
    }

    const auto& claims = auth_result.value();

    // 6. Check session consistency
    auto existing = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (existing && existing->value != claims.id) {
        result.emplace_back(net::outbound::OutgoingMessage{
            .target = net::outbound::Target::one(event->conn_id),
            .action =
                net::outbound::Action{std::in_place_type<net::outbound::DropConnection>,
                                      static_cast<int>(
                                          sercom::protocol::event::CommandErrorCode_FORBIDDEN),
                                      "Auth user mismatch"}});
        return result;
    }

    result.emplace_back(net::outbound::OutgoingMessage{
        .target = net::outbound::Target::one(event->conn_id),
        .action = net::outbound::Action{std::in_place_type<net::outbound::UpdateAuthState>,
                                        net::outbound::UpdateAuthState{
                                            .is_authenticated = true,
                                            .expires_at = std::chrono::system_clock::time_point{
                                                std::chrono::seconds{claims.exp}}}}});

    return result;
}
}  // namespace app
