#ifndef APP_COMMANDS_UTILS_H
#define APP_COMMANDS_UTILS_H

#include "domains/ids/Ids.h"
#include "net/outbound/Msg.h"

// Protobuf includes
#include "proto/command/message.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"

#include <string_view>

namespace app {

// Helper to create a command error outbound message
inline net::outbound::OutgoingMessage make_command_error(
    const GlobalConnId& conn, sercom::protocol::Envelope::Type type,
    sercom::protocol::event::CommandErrorCode code, std::string_view message) {
    sercom::protocol::event::CommandError err;
    err.set_command_type(type);
    err.set_code(code);
    if (!message.empty()) {
        err.set_message(message.data(), message.size());
    }

    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(sercom::protocol::Envelope::CommandError);
    err.SerializeToString(env.mutable_payload());

    std::string bytes;
    env.SerializeToString(&bytes);

    return net::outbound::OutgoingMessage{
        .target = net::outbound::Target::one(conn),
        .action = net::outbound::SendPayload{
            .payload = net::outbound::Payload{.data = std::move(bytes), .is_binary = true}}};
}

// Helper to create a command drop connection outbound message
inline net::outbound::OutgoingMessage make_drop_connection(
    const GlobalConnId& conn, sercom::protocol::event::CommandErrorCode code,
    std::string_view reason) {
    return net::outbound::OutgoingMessage{
        .target = net::outbound::Target::one(conn),
        .action = net::outbound::DropConnection{
            .code = static_cast<int>(code), .reason = std::string(reason.data(), reason.size())}};
}

}  // namespace app

#endif  // APP_COMMANDS_UTILS_H
