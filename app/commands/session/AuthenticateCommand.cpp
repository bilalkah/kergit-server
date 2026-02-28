#include "app/commands/session/AuthenticateCommand.h"

#include "app/commands/utils.h"
#include "app/proto_builders/EnvelopeBuilders.h"
#include "proto/command/session.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"
#include "proto/event/session.pb.h"

#include <chrono>
#include <optional>

namespace app {

namespace {

net::outbound::OutgoingMessage make_auth_ok(const GlobalConnId& conn) {
    sercom::protocol::event::AuthOk auth_ok;
    std::string bytes =
        proto_builders::serialize_envelope(sercom::protocol::Envelope::AUTH_OK, auth_ok);

    return net::outbound::OutgoingMessage{
        .target = net::outbound::Target::one(conn),
        .action = net::outbound::Action{
            std::in_place_type<net::outbound::SendPayload>,
            net::outbound::SendPayload{.payload = net::outbound::Payload{std::move(bytes), true}}}};
}

net::outbound::OutgoingMessage make_auth_failed(const GlobalConnId& conn) {
    return net::outbound::OutgoingMessage{
        .target = net::outbound::Target::one(conn),
        .action = net::outbound::Action{
            std::in_place_type<net::outbound::UpdateAuthState>,
            net::outbound::UpdateAuthState{
                .state = net::outbound::kAuthStateFailed,
                .expires_at = std::chrono::system_clock::time_point{},
                .user_id = std::nullopt}}};
}

}  // namespace

std::vector<net::outbound::OutgoingMessage> AuthenticateCommand::execute(CommandContext& ctx,
                                                                         const queue::Event& evt) {
    std::vector<net::outbound::OutgoingMessage> result;
    const auto* event = std::get_if<queue::MessageEvent>(&evt);
    if (!event) {
        return {};
    }

    const auto& env = event->payload.env;
    if (env.type() != sercom::protocol::Envelope::AUTH) {
        return {};
    }

    const auto& auth = require_parsed<sercom::protocol::command::Authenticate>(*event);

    // 4. Validate command fields
    if (auth.type() != sercom::protocol::command::AuthType_REAUTH &&
        auth.type() != sercom::protocol::command::AuthType_AUTH) {
        result.emplace_back(make_auth_failed(event->conn_id));
        result.emplace_back(net::outbound::OutgoingMessage{
            .target = net::outbound::Target::one(event->conn_id),
            .action = net::outbound::Action{
                std::in_place_type<net::outbound::DropConnection>,
                static_cast<int>(
                    sercom::protocol::event::CommandErrorCode::CommandErrorCode_INVALID_ARGUMENT),
                "Unsupported auth type"}});
        return result;
    }

    if (auth.provider() != sercom::protocol::command::AuthProvider_SUPABASE) {
        result.emplace_back(make_auth_failed(event->conn_id));
        result.emplace_back(net::outbound::OutgoingMessage{
            .target = net::outbound::Target::one(event->conn_id),
            .action = net::outbound::Action{
                std::in_place_type<net::outbound::DropConnection>,
                static_cast<int>(
                    sercom::protocol::event::CommandErrorCode::CommandErrorCode_INVALID_ARGUMENT),
                "Unsupported auth provider"}});
        return result;
    }

    if (auth.token().empty()) {
        result.emplace_back(make_auth_failed(event->conn_id));
        result.emplace_back(net::outbound::OutgoingMessage{
            .target = net::outbound::Target::one(event->conn_id),
            .action = net::outbound::Action{
                std::in_place_type<net::outbound::DropConnection>,
                static_cast<int>(
                    sercom::protocol::event::CommandErrorCode::CommandErrorCode_INVALID_ARGUMENT),
                "Token is required"}});
        return result;
    }

    auto auth_result = ctx.auth_service.authenticate(auth.token());
    if (!auth_result.has_value()) {
        result.emplace_back(make_auth_failed(event->conn_id));
        result.emplace_back(net::outbound::OutgoingMessage{
            .target = net::outbound::Target::one(event->conn_id),
            .action = net::outbound::Action{
                std::in_place_type<net::outbound::DropConnection>,
                static_cast<int>(sercom::protocol::event::CommandErrorCode_UNAUTHORIZED),
                "Authentication failed"}});
        return result;
    }

    const auto& claims = auth_result.value();
    const UserId user_id{claims.id};

    // If the connection is already authenticated, this is a re-auth.
    auto existing = ctx.session_manager.sessionOfConnection(event->conn_id);
    if (existing) {
        if (existing->value != claims.id) {
            result.emplace_back(make_auth_failed(event->conn_id));
            result.emplace_back(net::outbound::OutgoingMessage{
                .target = net::outbound::Target::one(event->conn_id),
                .action = net::outbound::Action{
                    std::in_place_type<net::outbound::DropConnection>,
                    static_cast<int>(sercom::protocol::event::CommandErrorCode_FORBIDDEN),
                    "Auth user mismatch"}});
            return result;
        }
        result.emplace_back(net::outbound::OutgoingMessage{
            .target = net::outbound::Target::one(event->conn_id),
            .action = net::outbound::Action{
                std::in_place_type<net::outbound::UpdateAuthState>,
                net::outbound::UpdateAuthState{
                    .state = net::outbound::kAuthStateAuthenticated,
                    .expires_at =
                        std::chrono::system_clock::time_point{std::chrono::seconds{claims.exp}},
                    .user_id = user_id}}});
        result.emplace_back(make_auth_ok(event->conn_id));
        return result;
    }

    auto attach_result = ctx.session_manager.attachConnection(event->conn_id, user_id);
    if (!attach_result.has_value()) {
        result.emplace_back(make_command_error(event->conn_id, env.type(), attach_result.error(),
                                               "Session limit exceeded"));
        return result;
    }

    result.emplace_back(net::outbound::OutgoingMessage{
        .target = net::outbound::Target::one(event->conn_id),
        .action = net::outbound::Action{
            std::in_place_type<net::outbound::UpdateAuthState>,
            net::outbound::UpdateAuthState{
                .state = net::outbound::kAuthStateAuthenticated,
                .expires_at =
                    std::chrono::system_clock::time_point{std::chrono::seconds{claims.exp}},
                .user_id = user_id}}});

    result.emplace_back(make_auth_ok(event->conn_id));

    queue::ConnectionEvent bootstrap_event;
    bootstrap_event.conn_id = event->conn_id;
    bootstrap_event.user_id = user_id;
    ctx.event_sink.push(std::move(bootstrap_event));

    return result;
}
}  // namespace app
