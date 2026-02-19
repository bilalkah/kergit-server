#ifndef NET_CONNECTION_OUTBOUND_MSG_H
#define NET_CONNECTION_OUTBOUND_MSG_H

#include "domains/ids/Ids.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace net::connection {
enum class AuthState : uint8_t;
}

namespace net::outbound {

enum class OutboundPriority : uint8_t {
    High = 0,
    Low = 1,
};

struct Payload {
    std::shared_ptr<const std::string> data;  // serialized bytes (shared)
    bool is_binary{true};                     // default for protobuf

    Payload() = default;
    explicit Payload(std::string bytes, bool binary = true)
        : data(std::make_shared<const std::string>(std::move(bytes))), is_binary(binary) {}
    explicit Payload(std::shared_ptr<const std::string> bytes, bool binary = true)
        : data(std::move(bytes)), is_binary(binary) {}
};

struct Target {
    std::vector<GlobalConnId> conns;

    static Target one(GlobalConnId c) { return {{std::move(c)}}; }

    static Target many(std::vector<GlobalConnId> cs) { return {std::move(cs)}; }
};

struct SendPayload {
    Payload payload;  // serialized, wire-ready
};

struct UpdateAuthState {
    connection::AuthState state{};
    std::chrono::system_clock::time_point expires_at{};
    std::optional<UserId> user_id{};
};

inline constexpr connection::AuthState kAuthStatePending =
    static_cast<connection::AuthState>(0);
inline constexpr connection::AuthState kAuthStateAuthenticated =
    static_cast<connection::AuthState>(1);
inline constexpr connection::AuthState kAuthStateFailed =
    static_cast<connection::AuthState>(2);

struct DropConnection {
    int code = 0;
    std::string reason;

    DropConnection() = default;
    DropConnection(int code_in, std::string reason_in = {})
        : code(code_in), reason(std::move(reason_in)) {}
    DropConnection(const DropConnection&) = default;
    DropConnection& operator=(const DropConnection&) = default;
    DropConnection(DropConnection&& other) noexcept
        : code(other.code), reason(std::move(other.reason)) {}
    DropConnection& operator=(DropConnection&& other) noexcept {
        if (this != &other) {
            code = other.code;
            reason = std::move(other.reason);
        }
        return *this;
    }
};

using Action = std::variant<SendPayload, UpdateAuthState, DropConnection>;

struct OutgoingMessage {
    OutboundPriority priority{OutboundPriority::High};
    Target target;
    Action action;
};

}  // namespace net::outbound
#endif  // NET_CONNECTION_OUTBOUND_MSG_H
