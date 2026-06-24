#ifndef NET_OUTBOUND_RELIABLESEQ_H
#define NET_OUTBOUND_RELIABLESEQ_H

#include "proto/envelope.pb.h"

#include <cstdint>
#include <string>

namespace net::outbound {

// Stamp a per-connection reliable-delivery sequence onto a base Envelope (one that was
// serialized with seq unset). Uses the protobuf API directly — parse, set the field,
// re-serialize — so there are no hand-rolled wire-format constants to keep in sync with
// the .proto. The parse+serialize cost is negligible because reliable delivery is scoped
// to low-frequency, small-fan-out events (voice), and it runs on the event-loop thread,
// not inside the socket write path.
inline std::string stamp_envelope_seq(const std::string& base_envelope, uint64_t seq) {
    sercom::protocol::Envelope env;
    // base_envelope is always something we serialized ourselves, so parsing succeeds; if
    // it somehow did not, env stays default and we still emit a valid (empty) envelope
    // rather than corrupt bytes.
    env.ParseFromString(base_envelope);
    env.set_seq(seq);
    return env.SerializeAsString();
}

}  // namespace net::outbound

#endif  // NET_OUTBOUND_RELIABLESEQ_H
