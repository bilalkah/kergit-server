#include "app/commands/session/AuthenticateCommand.h"

#include "app/commands/utils.h"
#include "proto/command/session.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/activity.pb.h"
#include "proto/event/error.pb.h"
#include "proto/event/session.pb.h"

#include <cassert>
#include <chrono>
#include <optional>

namespace app {

namespace {

net::outbound::OutgoingMessage make_auth_ok(net::outbound::Target target) {
    sercom::protocol::event::AuthOk auth_ok;
    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(sercom::protocol::Envelope::AUTH_OK);
    auth_ok.SerializeToString(env.mutable_payload());
    std::string bytes = env.SerializeAsString();

    return make_outgoing_message(std::move(target), std::move(bytes));
}

net::outbound::OutgoingMessage make_auth_update(net::outbound::Target target,
                                                const net::connection::AuthState auth_state,
                                                std::optional<UserId> u_id = std::nullopt,
                                                int64_t expires_at_unix = 0) {
    return net::outbound::OutgoingMessage{
        .target = std::move(target),
        .action = net::outbound::Action{
            std::in_place_type<net::outbound::UpdateAuthState>,
            net::outbound::UpdateAuthState{
                .state = auth_state,
                .expires_at =
                    std::chrono::system_clock::time_point{std::chrono::seconds{expires_at_unix}},
                .user_id = std::move(u_id)}}};
}

std::string make_voice_self_status_disconnected() {
    sercom::protocol::event::VoiceSelfStatus status;
    status.set_connected(false);
    status.set_is_owner(false);

    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(sercom::protocol::Envelope::VOICE_SELF_STATUS);
    status.SerializeToString(env.mutable_payload());
    return env.SerializeAsString();
}

std::string make_voice_self_revoked() {
    sercom::protocol::event::VoiceSelfRevoked revoked;

    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(sercom::protocol::Envelope::VOICE_SELF_REVOKED);
    revoked.SerializeToString(env.mutable_payload());
    return env.SerializeAsString();
}

}  // namespace

std::vector<net::outbound::OutgoingMessage> AuthenticateCommand::execute(CommandContext& ctx,
                                                                         const queue::Event& evt) {
    std::vector<net::outbound::OutgoingMessage> result;
    assert(std::holds_alternative<queue::MessageEvent>(evt));
    const auto& event = std::get<queue::MessageEvent>(evt);

    const auto& env = event.payload.env;
    assert(env.type() == sercom::protocol::Envelope::AUTH);

    const auto& auth = require_parsed<sercom::protocol::command::Authenticate>(event);

    // 4. Validate command fields
    if (auth.type() != sercom::protocol::command::AuthType_REAUTH &&
        auth.type() != sercom::protocol::command::AuthType_AUTH) {
        result.emplace_back(make_auth_update(net::outbound::Target::one(event.conn_id),
                                             net::outbound::kAuthStateFailed));
        return result;
    }

    if (auth.provider() != sercom::protocol::command::AuthProvider_SUPABASE) {
        result.emplace_back(make_auth_update(net::outbound::Target::one(event.conn_id),
                                             net::outbound::kAuthStateFailed));
        return result;
    }

    if (auth.token().empty()) {
        result.emplace_back(make_auth_update(net::outbound::Target::one(event.conn_id),
                                             net::outbound::kAuthStateFailed));
        return result;
    }

    auto auth_result = ctx.auth_service.authenticate(auth.token());
    if (!auth_result.has_value()) {
        ctx.audit_service.log(AuditRepository::Event{
            .category = "auth",
            .event_type = "auth.login_failed",
            .severity = "info",
            .actor_type = "user",
            .connection_id = to_string(event.conn_id),
        });
        
        result.emplace_back(make_auth_update(net::outbound::Target::one(event.conn_id),
                                             net::outbound::kAuthStateFailed));
        return result;
    }

    const auto& claims = auth_result.value();
    const UserId user_id{claims.id};

    // If the connection is already authenticated, this is a re-auth.
    auto existing = ctx.session_manager.sessionOfConnection(event.conn_id);
    if (existing) {
        if (existing->value != claims.id) {
            result.emplace_back(make_auth_update(net::outbound::Target::one(event.conn_id),
                                                 net::outbound::kAuthStateFailed));
            return result;
        }
        result.emplace_back(make_auth_update(net::outbound::Target::one(event.conn_id),
                                             net::outbound::kAuthStateAuthenticated, user_id,
                                             claims.exp));
        result.emplace_back(make_auth_ok(net::outbound::Target::one(event.conn_id)));
        return result;
    }

    auto attach_result = ctx.session_manager.attachConnection(event.conn_id, user_id);
    if (!attach_result.has_value()) {
        result.emplace_back(make_command_error(event.conn_id, env.type(), attach_result.error(),
                                               "Session limit exceeded"));
        return result;
    }
    const SessionId new_session_id = attach_result.value();

    if (auth.has_voice_resume()) {
        const auto scope_opt = to_channel_scope(auth.voice_resume().channel());
        if (scope_opt.has_value()) {
            const auto transfer = ctx.voice_service.try_resume_voice_ownership(
                user_id, scope_opt->hub_id, scope_opt->channel_id, auth.voice_resume().resume_id(),
                new_session_id);
            if (transfer.has_value() && transfer->previous_owner_session != new_session_id) {
                auto old_conns =
                    ctx.session_manager.getSessionIdConnections(transfer->previous_owner_session);
                if (!old_conns.empty()) {
                    auto revoked_targets = old_conns;
                    result.emplace_back(make_outgoing_message(
                        net::outbound::Target::many(std::move(revoked_targets)),
                        make_voice_self_revoked()));

                    result.emplace_back(
                        make_outgoing_message(net::outbound::Target::many(std::move(old_conns)),
                                              make_voice_self_status_disconnected()));
                }

                // Re-sync the current E2EE key to the resumed owner in case a rotation
                // landed during the reconnect window and this session missed it.
                ctx.voice_service.resync_voice_key_for_user(user_id);
            }
        }
    }

    result.emplace_back(make_auth_update(net::outbound::Target::one(event.conn_id),
                                         net::outbound::kAuthStateAuthenticated, user_id,
                                         claims.exp));

    result.emplace_back(make_auth_ok(net::outbound::Target::one(event.conn_id)));

    queue::ConnectionEvent bootstrap_event;
    bootstrap_event.conn_id = event.conn_id;
    bootstrap_event.user_id = user_id;
    ctx.event_sink.push(std::move(bootstrap_event));

    ctx.audit_service.log(AuditRepository::Event{
        .category = "auth",
        .event_type = "auth.login_succeeded",
        .severity = "info",
        .actor_type = "user",
        .actor_user_id = user_id,
        .session_id = std::to_string(new_session_id),
        .connection_id = to_string(event.conn_id),
    });

    return result;
}
}  // namespace app
