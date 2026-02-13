#pragma once

#include "proto/envelope.pb.h"

#include <string>

namespace app::proto_builders {

template <typename T>
inline std::string serialize_envelope(sercom::protocol::Envelope::Type type, const T& message) {
    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(type);
    message.SerializeToString(env.mutable_payload());
    return env.SerializeAsString();
}

}  // namespace app::proto_builders
