#include "net/outbound/ReliableSeq.h"

#include "proto/envelope.pb.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <string>

namespace {

// Build a base Envelope (no seq) the same way the app layer does for reliable sends.
std::string base_envelope(sercom::protocol::Envelope::Type type, const std::string& payload) {
    sercom::protocol::Envelope env;
    env.set_version(1);
    env.set_type(type);
    env.set_payload(payload);
    return env.SerializeAsString();
}

TEST(ReliableSeq, StampsParseableSeqWithoutDisturbingOtherFields) {
    const std::string base =
        base_envelope(sercom::protocol::Envelope::STATE_DELTA, "hello-payload");

    const std::string wire = net::outbound::stamp_envelope_seq(base, 42);

    sercom::protocol::Envelope parsed;
    ASSERT_TRUE(parsed.ParseFromString(wire));
    EXPECT_EQ(parsed.seq(), 42u);
    EXPECT_EQ(parsed.version(), 1u);
    EXPECT_EQ(parsed.type(), sercom::protocol::Envelope::STATE_DELTA);
    EXPECT_EQ(parsed.payload(), "hello-payload");
}

TEST(ReliableSeq, BaseHasNoSeqButStampedDoes) {
    const std::string base = base_envelope(sercom::protocol::Envelope::VOICE_KEY_UPDATE, "k");

    sercom::protocol::Envelope base_parsed;
    ASSERT_TRUE(base_parsed.ParseFromString(base));
    EXPECT_EQ(base_parsed.seq(), 0u);  // unsequenced base

    sercom::protocol::Envelope stamped;
    ASSERT_TRUE(stamped.ParseFromString(net::outbound::stamp_envelope_seq(base, 7)));
    EXPECT_EQ(stamped.seq(), 7u);
}

TEST(ReliableSeq, EncodesMultiByteVarintSeq) {
    const std::string base = base_envelope(sercom::protocol::Envelope::STATE_DELTA, "p");

    // Values that span 1, 2, 3 and many varint bytes, plus the 64-bit max.
    const uint64_t seqs[] = {uint64_t{1},       uint64_t{127},     uint64_t{128},
                             uint64_t{300},      uint64_t{16384},   uint64_t{1} << 32,
                             std::numeric_limits<uint64_t>::max()};
    for (uint64_t seq : seqs) {
        sercom::protocol::Envelope parsed;
        ASSERT_TRUE(parsed.ParseFromString(net::outbound::stamp_envelope_seq(base, seq)))
            << "seq=" << seq;
        EXPECT_EQ(parsed.seq(), seq) << "seq=" << seq;
        EXPECT_EQ(parsed.payload(), "p");
    }
}

TEST(ReliableSeq, MonotonicSeqsRemainDistinctAndOrdered) {
    const std::string base = base_envelope(sercom::protocol::Envelope::STATE_DELTA, "x");
    uint64_t prev = 0;
    for (uint64_t i = 1; i <= 1000; ++i) {
        sercom::protocol::Envelope parsed;
        ASSERT_TRUE(parsed.ParseFromString(net::outbound::stamp_envelope_seq(base, i)));
        EXPECT_EQ(parsed.seq(), i);
        EXPECT_GT(parsed.seq(), prev);
        prev = parsed.seq();
    }
}

}  // namespace
