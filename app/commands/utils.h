#ifndef APP_COMMANDS_UTILS_H
#define APP_COMMANDS_UTILS_H

#include "app/queue/Msg.h"
#include "domains/ids/Ids.h"
#include "net/outbound/Msg.h"
#include "utils/Logger.h"
#include "utils/Metrics.h"

// Protobuf includes
#include "proto/command/message.pb.h"
#include "proto/envelope.pb.h"
#include "proto/event/error.pb.h"

#include <exception>
#include <string_view>
#include <typeinfo>
#include <vector>

namespace app {

template <typename T>
inline const T& require_parsed(const queue::MessageEvent& event) {
    const auto* cmd = std::get_if<T>(&event.payload.parsed);
    if (cmd) {
        return *cmd;
    }
    utils::metrics::counters().parsed_payload_violation_total.fetch_add(
        1, std::memory_order_relaxed);
    utils::log_line(utils::LogLevel::ERROR,
                    std::string("Parsed payload invariant violated for command ") +
                        typeid(T).name());
    std::terminate();
}

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
        .action = net::outbound::Action{std::in_place_type<net::outbound::SendPayload>,
                                        net::outbound::SendPayload{
                                            .payload = net::outbound::Payload{std::move(bytes),
                                                                              true}}}};
}

// Helper to create a command drop connection outbound message
inline net::outbound::OutgoingMessage make_drop_connection(
    const GlobalConnId& conn, sercom::protocol::event::CommandErrorCode code,
    std::string_view reason) {
    return net::outbound::OutgoingMessage{
        .target = net::outbound::Target::one(conn),
        .action = net::outbound::Action{std::in_place_type<net::outbound::DropConnection>,
                                        static_cast<int>(code),
                                        std::string(reason.data(), reason.size())}};
}

inline std::vector<net::outbound::OutgoingMessage> single_outgoing(
    net::outbound::OutgoingMessage msg) {
    std::vector<net::outbound::OutgoingMessage> out;
    out.emplace_back(std::move(msg));
    return out;
}

}  // namespace app

#endif  // APP_COMMANDS_UTILS_H
